#include "tg/pty.h"
#include <atomic>
#include <mutex>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#if defined( __APPLE__ )
#include <util.h>
#else
#include <pty.h>
#endif
#endif

namespace tg
{
#ifdef _WIN32
    // ConPTY is loaded at runtime so tg still runs (without terminals) on older Windows.
    using HPCON_ = void*;
    using CreatePC_t = HRESULT( WINAPI* )( COORD, HANDLE, HANDLE, DWORD, HPCON_* );
    using ResizePC_t = HRESULT( WINAPI* )( HPCON_, COORD );
    using ClosePC_t = void( WINAPI* )( HPCON_ );
    struct ConPtyApi
    {
        CreatePC_t Create = nullptr;
        ResizePC_t Resize = nullptr;
        ClosePC_t Close = nullptr;
        ConPtyApi()
        {
            HMODULE K = GetModuleHandleW( L"kernel32.dll" );
            Create = (CreatePC_t)(void*)GetProcAddress( K, "CreatePseudoConsole" );
            Resize = (ResizePC_t)(void*)GetProcAddress( K, "ResizePseudoConsole" );
            Close = (ClosePC_t)(void*)GetProcAddress( K, "ClosePseudoConsole" );
        }
    };
    static ConPtyApi& Api() { static ConPtyApi A; return A; }

    struct Pty::Impl
    {
        HPCON_ Con = nullptr;
        std::mutex ConLock; // the waiter thread closes it when the process exits; Resize uses it on the UI thread
        HANDLE InWrite = nullptr, OutRead = nullptr;
        PROCESS_INFORMATION Proc{};
        std::thread Reader, Waiter;
        std::atomic<bool> Alive{ false };
        void CloseCon()
        {
            std::lock_guard<std::mutex> L( ConLock );
            if ( Con ) Api().Close( Con );
            Con = nullptr;
        }
    };

    bool Pty::Supported() { return Api().Create != nullptr; }

    static std::wstring Widen( const std::string& S )
    {
        int N = MultiByteToWideChar( CP_UTF8, 0, S.c_str(), -1, nullptr, 0 );
        std::wstring W( size_t( N > 0 ? N - 1 : 0 ), L'\0' );
        if ( N > 1 ) MultiByteToWideChar( CP_UTF8, 0, S.c_str(), -1, W.data(), N );
        return W;
    }

    bool Pty::Start( const PtyOptions& O, std::function<void( std::string )> OnOutput, std::function<void( int )> OnExit )
    {
        if ( P->Alive || !Supported() ) return false;
        HANDLE InRead = nullptr, OutWrite = nullptr;
        if ( !CreatePipe( &InRead, &P->InWrite, nullptr, 0 ) || !CreatePipe( &P->OutRead, &OutWrite, nullptr, 0 ) ) return false;
        COORD Size = { SHORT( O.Cols ), SHORT( O.Rows ) };
        HPCON_ Con = nullptr;
        if ( FAILED( Api().Create( Size, InRead, OutWrite, 0, &Con ) ) ) return false;
        P->Con = Con;
        CloseHandle( InRead );   // the console owns these now
        CloseHandle( OutWrite );

        STARTUPINFOEXW Si{};
        Si.StartupInfo.cb = sizeof( Si );
        // Without this, a host whose own stdio is redirected (launched from a pipe, an IDE, a service) hands
        // those handles to the child and its output bypasses the pseudo console.
        Si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        SIZE_T AttrSize = 0;
        InitializeProcThreadAttributeList( nullptr, 1, 0, &AttrSize );
        std::vector<char> AttrBuf( AttrSize );
        Si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)AttrBuf.data();
        InitializeProcThreadAttributeList( Si.lpAttributeList, 1, 0, &AttrSize );
        UpdateProcThreadAttribute( Si.lpAttributeList, 0, 0x00020016 /* PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE */, Con, sizeof( Con ), nullptr, nullptr );

        for ( auto& [ K, V ] : O.Env ) SetEnvironmentVariableW( Widen( K ).c_str(), Widen( V ).c_str() );
        std::wstring Cmd = O.Command.empty() ? L"cmd.exe" : L"cmd.exe /c " + Widen( O.Command );
        std::wstring Cwd = Widen( O.Cwd );
        BOOL Ok = CreateProcessW( nullptr, Cmd.data(), nullptr, nullptr, FALSE, EXTENDED_STARTUPINFO_PRESENT, nullptr,
                                  Cwd.empty() ? nullptr : Cwd.c_str(), &Si.StartupInfo, &P->Proc );
        DeleteProcThreadAttributeList( Si.lpAttributeList );
        if ( !Ok ) { P->CloseCon(); return false; }

        P->Alive = true;
        Impl* I = P.get();
        P->Reader = std::thread( [ I, OnOutput, OnExit ] {
            char Buf[ 8192 ];
            DWORD N = 0;
            while ( ReadFile( I->OutRead, Buf, sizeof( Buf ), &N, nullptr ) && N > 0 )
                if ( OnOutput ) OnOutput( std::string( Buf, N ) );
            DWORD Code = 0;
            WaitForSingleObject( I->Proc.hProcess, 2000 );
            GetExitCodeProcess( I->Proc.hProcess, &Code );
            I->Alive = false;
            if ( OnExit ) OnExit( int( Code ) );
        } );
        // ConPTY keeps the output pipe open after the child exits: close the console when it does.
        P->Waiter = std::thread( [ I ] {
            WaitForSingleObject( I->Proc.hProcess, INFINITE );
            I->CloseCon();
        } );
        return true;
    }

    void Pty::Write( std::string_view B )
    {
        if ( !P->Alive || !P->InWrite ) return;
        DWORD N = 0;
        WriteFile( P->InWrite, B.data(), DWORD( B.size() ), &N, nullptr );
    }

    void Pty::Resize( int C, int R )
    {
        std::lock_guard<std::mutex> L( P->ConLock );
        if ( P->Con && Api().Resize ) Api().Resize( P->Con, COORD{ SHORT( C ), SHORT( R ) } );
    }

    void Pty::Kill()
    {
        if ( P->Proc.hProcess ) TerminateProcess( P->Proc.hProcess, 1 ); // harmless if it already exited
        if ( P->Waiter.joinable() ) P->Waiter.join(); // the process is gone (or terminated): it returns
        P->CloseCon();
        if ( P->Reader.joinable() ) P->Reader.join();
        if ( P->InWrite ) { CloseHandle( P->InWrite ); P->InWrite = nullptr; }
        if ( P->OutRead ) { CloseHandle( P->OutRead ); P->OutRead = nullptr; }
        if ( P->Proc.hProcess ) { CloseHandle( P->Proc.hProcess ); CloseHandle( P->Proc.hThread ); P->Proc = {}; }
    }

#else // ---------------------------------------------------------------- POSIX

    struct Pty::Impl
    {
        int Master = -1;
        pid_t Pid = -1;
        std::thread Reader;
        std::atomic<bool> Alive{ false };
    };

    bool Pty::Supported() { return true; }

    bool Pty::Start( const PtyOptions& O, std::function<void( std::string )> OnOutput, std::function<void( int )> OnExit )
    {
        if ( P->Alive ) return false;
        winsize Ws{};
        Ws.ws_col = (unsigned short)O.Cols;
        Ws.ws_row = (unsigned short)O.Rows;
        int Master = -1;
        pid_t Pid = forkpty( &Master, nullptr, nullptr, &Ws );
        if ( Pid < 0 ) return false;
        if ( Pid == 0 )
        {
            // child: becomes the program on the terminal's slave side
            setenv( "TERM", "xterm-256color", 1 );
            setenv( "COLORTERM", "truecolor", 1 );
            for ( auto& [ K, V ] : O.Env ) setenv( K.c_str(), V.c_str(), 1 );
            if ( !O.Cwd.empty() && chdir( O.Cwd.c_str() ) != 0 ) _exit( 126 );
            const char* Shell = getenv( "SHELL" );
            if ( !Shell || !*Shell ) Shell = "/bin/sh";
            if ( O.Command.empty() ) execl( Shell, Shell, "-i", (char*)nullptr );
            else execl( "/bin/sh", "sh", "-c", O.Command.c_str(), (char*)nullptr );
            _exit( 127 );
        }
        P->Master = Master;
        P->Pid = Pid;
        P->Alive = true;
        Impl* I = P.get();
        P->Reader = std::thread( [ I, OnOutput, OnExit ] {
            char Buf[ 8192 ];
            for ( ;; )
            {
                ssize_t N = read( I->Master, Buf, sizeof( Buf ) );
                if ( N > 0 ) { if ( OnOutput ) OnOutput( std::string( Buf, size_t( N ) ) ); continue; }
                if ( N < 0 && errno == EINTR ) continue;
                break; // EIO: the child side closed (process exited)
            }
            int Status = 0;
            waitpid( I->Pid, &Status, 0 );
            I->Alive = false;
            if ( OnExit ) OnExit( WIFEXITED( Status ) ? WEXITSTATUS( Status ) : 128 + ( WIFSIGNALED( Status ) ? WTERMSIG( Status ) : 0 ) );
        } );
        return true;
    }

    void Pty::Write( std::string_view B )
    {
        if ( !P->Alive || P->Master < 0 ) return;
        while ( !B.empty() )
        {
            ssize_t N = write( P->Master, B.data(), B.size() );
            if ( N < 0 ) { if ( errno == EINTR || errno == EAGAIN ) continue; break; }
            B.remove_prefix( size_t( N ) );
        }
    }

    void Pty::Resize( int C, int R )
    {
        if ( P->Master < 0 ) return;
        winsize Ws{};
        Ws.ws_col = (unsigned short)C;
        Ws.ws_row = (unsigned short)R;
        ioctl( P->Master, TIOCSWINSZ, &Ws ); // the kernel sends SIGWINCH to the program
    }

    void Pty::Kill()
    {
        if ( P->Pid > 0 && P->Alive ) { kill( P->Pid, SIGHUP ); kill( P->Pid, SIGKILL ); }
        if ( P->Reader.joinable() ) P->Reader.join();
        if ( P->Master >= 0 ) { close( P->Master ); P->Master = -1; }
        P->Pid = -1;
    }
#endif

    Pty::Pty() : P( std::make_unique<Impl>() ) {}
    Pty::~Pty() { Kill(); }
    bool Pty::Running() const { return P->Alive; }
}
