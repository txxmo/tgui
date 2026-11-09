// tg::os — native dialogs, notifications, clipboard images, tray icon. See tg/os.h.
#include "tg/os.h"
#include "tg/context.h"
#include <algorithm>
#include <cstring>
#include <thread>

#if defined( _WIN32 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <shobjidl.h>
#else
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

#if !defined( _WIN32 ) && !defined( __APPLE__ )
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#if defined( __GNUC__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wshadow"
#endif
#include "stb_image.h"
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#if defined( __GNUC__ )
#pragma GCC diagnostic pop
#endif
#endif

namespace tg::os
{
    // ---- async wrappers: same on every platform
    void OpenFilesAsync( Context& Ctx, FileDialog D, std::function<void( std::vector<std::string> )> Done )
    {
        Context* C = &Ctx;
        std::thread( [ C, D = std::move( D ), Done = std::move( Done ) ] {
            auto R = OpenFiles( D );
            C->Post( [ Done, R = std::move( R ) ]() mutable { Done( std::move( R ) ); } );
        } ).detach();
    }
    void SaveFileAsync( Context& Ctx, FileDialog D, std::function<void( std::string )> Done )
    {
        Context* C = &Ctx;
        std::thread( [ C, D = std::move( D ), Done = std::move( Done ) ] {
            auto R = SaveFile( D );
            C->Post( [ Done, R = std::move( R ) ]() mutable { Done( std::move( R ) ); } );
        } ).detach();
    }
    void PickFolderAsync( Context& Ctx, FileDialog D, std::function<void( std::string )> Done )
    {
        Context* C = &Ctx;
        std::thread( [ C, D = std::move( D ), Done = std::move( Done ) ] {
            auto R = PickFolder( D );
            C->Post( [ Done, R = std::move( R ) ]() mutable { Done( std::move( R ) ); } );
        } ).detach();
    }

    std::string OpenFile( const FileDialog& D )
    {
        FileDialog One = D;
        One.Multiple = false;
        auto R = OpenFiles( One );
        return R.empty() ? std::string() : R[ 0 ];
    }

    static std::vector<std::string> SplitPatterns( const std::string& P ) // "*.png;*.jpg" / "*.png *.jpg"
    {
        std::vector<std::string> Out;
        std::string Cur;
        for ( char C : P )
        {
            if ( C == ';' || C == ' ' || C == ',' )
            {
                if ( !Cur.empty() ) Out.push_back( Cur );
                Cur.clear();
            }
            else Cur += C;
        }
        if ( !Cur.empty() ) Out.push_back( Cur );
        return Out;
    }
}

#if defined( _WIN32 )
// ============================================================================ Windows

namespace tg::os
{
    static std::wstring Widen( const std::string& S )
    {
        int N = MultiByteToWideChar( CP_UTF8, 0, S.c_str(), -1, nullptr, 0 );
        std::wstring W( size_t( N > 0 ? N - 1 : 0 ), L'\0' );
        if ( N > 1 ) MultiByteToWideChar( CP_UTF8, 0, S.c_str(), -1, W.data(), N );
        return W;
    }
    static std::string Narrow( const wchar_t* W )
    {
        int N = WideCharToMultiByte( CP_UTF8, 0, W, -1, nullptr, 0, nullptr, nullptr );
        std::string S( size_t( N > 0 ? N - 1 : 0 ), '\0' );
        if ( N > 1 ) WideCharToMultiByte( CP_UTF8, 0, W, -1, S.data(), N, nullptr, nullptr );
        return S;
    }

    struct ComScope
    {
        HRESULT Hr;
        ComScope() : Hr( CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE ) ) {}
        ~ComScope() { if ( SUCCEEDED( Hr ) ) CoUninitialize(); }
    };

    bool DialogsAvailable() { return true; }

    enum class Kind { Open, Save, Folder };

    static std::vector<std::string> RunDialog( const FileDialog& D, Kind K )
    {
        std::vector<std::string> Out;
        ComScope Com;
        IFileDialog* Dlg = nullptr;
        HRESULT Hr = K == Kind::Save ? CoCreateInstance( CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileSaveDialog, (void**)&Dlg )
                                     : CoCreateInstance( CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileOpenDialog, (void**)&Dlg );
        if ( FAILED( Hr ) || !Dlg ) return Out;
        DWORD Opt = 0;
        Dlg->GetOptions( &Opt );
        Opt |= FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR;
        if ( K == Kind::Folder ) Opt |= FOS_PICKFOLDERS;
        if ( K == Kind::Open && D.Multiple ) Opt |= FOS_ALLOWMULTISELECT;
        if ( K == Kind::Open ) Opt |= FOS_FILEMUSTEXIST;
        if ( K == Kind::Save ) Opt |= FOS_OVERWRITEPROMPT;
        Dlg->SetOptions( Opt );
        if ( !D.Title.empty() ) Dlg->SetTitle( Widen( D.Title ).c_str() );
        std::vector<std::wstring> Names, Specs;
        std::vector<COMDLG_FILTERSPEC> Fs;
        if ( K != Kind::Folder && !D.Filters.empty() )
        {
            for ( auto& F : D.Filters )
            {
                std::string Sp;
                for ( auto& P : SplitPatterns( F.Patterns ) ) Sp += ( Sp.empty() ? "" : ";" ) + P;
                Names.push_back( Widen( F.Name ) );
                Specs.push_back( Widen( Sp.empty() ? "*.*" : Sp ) );
            }
            for ( size_t i = 0; i < Names.size(); i++ ) Fs.push_back( { Names[ i ].c_str(), Specs[ i ].c_str() } );
            Dlg->SetFileTypes( UINT( Fs.size() ), Fs.data() );
            if ( K == Kind::Save ) // default extension from the first filter ("*.csv" -> "csv")
            {
                auto P = SplitPatterns( D.Filters[ 0 ].Patterns );
                if ( !P.empty() && P[ 0 ].rfind( "*.", 0 ) == 0 && P[ 0 ].size() > 2 && P[ 0 ].find( '*', 1 ) == std::string::npos )
                    Dlg->SetDefaultExtension( Widen( P[ 0 ].substr( 2 ) ).c_str() );
            }
        }
        if ( !D.Name.empty() ) Dlg->SetFileName( Widen( D.Name ).c_str() );
        if ( !D.Folder.empty() )
        {
            IShellItem* Folder = nullptr;
            if ( SUCCEEDED( SHCreateItemFromParsingName( Widen( D.Folder ).c_str(), nullptr, IID_IShellItem, (void**)&Folder ) ) && Folder )
            {
                Dlg->SetFolder( Folder );
                Folder->Release();
            }
        }
        if ( SUCCEEDED( Dlg->Show( (HWND)D.Owner ) ) )
        {
            auto Take = [ & ]( IShellItem* It ) {
                PWSTR Path = nullptr;
                if ( It && SUCCEEDED( It->GetDisplayName( SIGDN_FILESYSPATH, &Path ) ) && Path )
                {
                    Out.push_back( Narrow( Path ) );
                    CoTaskMemFree( Path );
                }
            };
            IShellItemArray* Arr = nullptr;
            if ( K == Kind::Open && SUCCEEDED( ( (IFileOpenDialog*)Dlg )->GetResults( &Arr ) ) && Arr )
            {
                DWORD N = 0;
                Arr->GetCount( &N );
                for ( DWORD i = 0; i < N; i++ )
                {
                    IShellItem* It = nullptr;
                    if ( SUCCEEDED( Arr->GetItemAt( i, &It ) ) ) { Take( It ); It->Release(); }
                }
                Arr->Release();
            }
            else
            {
                IShellItem* It = nullptr;
                if ( SUCCEEDED( Dlg->GetResult( &It ) ) && It ) { Take( It ); It->Release(); }
            }
        }
        Dlg->Release();
        return Out;
    }

    std::vector<std::string> OpenFiles( const FileDialog& D ) { return RunDialog( D, Kind::Open ); }
    std::string SaveFile( const FileDialog& D )
    {
        auto R = RunDialog( D, Kind::Save );
        return R.empty() ? std::string() : R[ 0 ];
    }
    std::string PickFolder( const FileDialog& D )
    {
        auto R = RunDialog( D, Kind::Folder );
        return R.empty() ? std::string() : R[ 0 ];
    }

    bool OpenUrl( const std::string& U )
    {
        return (INT_PTR)ShellExecuteW( nullptr, L"open", Widen( U ).c_str(), nullptr, nullptr, SW_SHOWNORMAL ) > 32;
    }

    // ---- clipboard images: CF_DIB (what screenshots and browsers put there)
    bool GetClipboardImage( Bitmap& Out )
    {
        if ( !OpenClipboard( nullptr ) ) return false;
        bool Ok = false;
        if ( HANDLE H = GetClipboardData( CF_DIB ) )
            if ( auto* Bi = (const BITMAPINFOHEADER*)GlobalLock( H ) )
            {
                SIZE_T Size = GlobalSize( H );
                int W = Bi->biWidth, Hh = Bi->biHeight;
                bool TopDown = Hh < 0;
                Hh = Hh < 0 ? -Hh : Hh;
                int Bpp = Bi->biBitCount;
                size_t Head = Bi->biSize;
                if ( Bi->biCompression == BI_BITFIELDS ) Head += 12;
                if ( Bpp <= 8 ) Head += size_t( Bi->biClrUsed ? Bi->biClrUsed : ( 1u << Bpp ) ) * 4;
                size_t Stride = ( size_t( W ) * size_t( Bpp ) + 31 ) / 32 * 4;
                if ( W > 0 && Hh > 0 && ( Bpp == 24 || Bpp == 32 ) && Head + Stride * size_t( Hh ) <= Size &&
                     ( Bi->biCompression == BI_RGB || Bi->biCompression == BI_BITFIELDS ) )
                {
                    const uint8_t* Px = (const uint8_t*)Bi + Head;
                    Out.Width = W;
                    Out.Height = Hh;
                    Out.Pixels.resize( size_t( W ) * size_t( Hh ) );
                    bool AnyAlpha = false;
                    for ( int y = 0; y < Hh; y++ )
                    {
                        const uint8_t* Row = Px + Stride * size_t( TopDown ? y : Hh - 1 - y );
                        for ( int x = 0; x < W; x++ )
                        {
                            const uint8_t* P = Row + size_t( x ) * size_t( Bpp / 8 );
                            uint8_t A = Bpp == 32 ? P[ 3 ] : 255;
                            AnyAlpha |= A != 0;
                            Out.Pixels[ size_t( y ) * W + x ] = Color( P[ 2 ], P[ 1 ], P[ 0 ], A ).Pack();
                        }
                    }
                    if ( !AnyAlpha ) // 32-bit DIBs often leave alpha at 0: treat as opaque
                        for ( auto& P : Out.Pixels ) P |= 0xFF000000u;
                    Ok = true;
                }
                GlobalUnlock( H );
            }
        CloseClipboard();
        return Ok;
    }

    bool SetClipboardImage( const Bitmap& Img )
    {
        if ( Img.Empty() || Img.Pixels.size() < size_t( Img.Width ) * Img.Height ) return false;
        size_t Bytes = sizeof( BITMAPINFOHEADER ) + size_t( Img.Width ) * Img.Height * 4;
        HGLOBAL G = GlobalAlloc( GMEM_MOVEABLE, Bytes );
        if ( !G ) return false;
        auto* Bi = (BITMAPINFOHEADER*)GlobalLock( G );
        *Bi = {};
        Bi->biSize = sizeof( BITMAPINFOHEADER );
        Bi->biWidth = Img.Width;
        Bi->biHeight = Img.Height; // bottom-up: what most readers expect
        Bi->biPlanes = 1;
        Bi->biBitCount = 32;
        Bi->biCompression = BI_RGB;
        uint8_t* Px = (uint8_t*)( Bi + 1 );
        for ( int y = 0; y < Img.Height; y++ )
            for ( int x = 0; x < Img.Width; x++ )
            {
                Color C = Color::Unpack( Img.Pixels[ size_t( y ) * Img.Width + x ] );
                uint8_t* P = Px + ( size_t( Img.Height - 1 - y ) * Img.Width + x ) * 4;
                P[ 0 ] = C.b; P[ 1 ] = C.g; P[ 2 ] = C.r; P[ 3 ] = C.a;
            }
        GlobalUnlock( G );
        if ( !OpenClipboard( nullptr ) ) { GlobalFree( G ); return false; }
        EmptyClipboard();
        bool Ok = SetClipboardData( CF_DIB, G ) != nullptr;
        if ( !Ok ) GlobalFree( G );
        CloseClipboard();
        return Ok;
    }
}

// ---- tray icon (and balloon notifications)
namespace tg
{
    static const UINT TrayMsg = WM_APP + 0x7A1;
    static const wchar_t* TrayClass = L"tg_tray";

    static HICON MakeIcon( const Bitmap& B )
    {
        if ( B.Empty() ) return nullptr;
        BITMAPV5HEADER H = {};
        H.bV5Size = sizeof( H );
        H.bV5Width = B.Width;
        H.bV5Height = -B.Height;
        H.bV5Planes = 1;
        H.bV5BitCount = 32;
        H.bV5Compression = BI_BITFIELDS;
        H.bV5RedMask = 0x00FF0000;
        H.bV5GreenMask = 0x0000FF00;
        H.bV5BlueMask = 0x000000FF;
        H.bV5AlphaMask = 0xFF000000;
        void* Bits = nullptr;
        HDC Dc = GetDC( nullptr );
        HBITMAP Color = CreateDIBSection( Dc, (BITMAPINFO*)&H, DIB_RGB_COLORS, &Bits, nullptr, 0 );
        ReleaseDC( nullptr, Dc );
        if ( !Color ) return nullptr;
        auto* Px = (uint32_t*)Bits;
        for ( size_t i = 0; i < size_t( B.Width ) * B.Height; i++ )
        {
            tg::Color C = tg::Color::Unpack( B.Pixels[ i ] );
            Px[ i ] = uint32_t( C.a ) << 24 | uint32_t( C.r ) << 16 | uint32_t( C.g ) << 8 | C.b;
        }
        HBITMAP Mask = CreateBitmap( B.Width, B.Height, 1, 1, nullptr );
        ICONINFO Ii = { TRUE, 0, 0, Mask, Color };
        HICON I = CreateIconIndirect( &Ii );
        DeleteObject( Color );
        DeleteObject( Mask );
        return I;
    }

    struct TrayIcon::Impl
    {
        TrayIcon* Self = nullptr;
        HWND Wnd = nullptr;
        NOTIFYICONDATAW Nid = {};
        HICON Own = nullptr;
        bool Added = false;
        UINT TaskbarCreated = 0;

        static LRESULT CALLBACK Proc( HWND H, UINT M, WPARAM W, LPARAM L )
        {
            auto* I = (Impl*)GetWindowLongPtrW( H, GWLP_USERDATA );
            if ( I && M == TrayMsg )
            {
                UINT Ev = LOWORD( L );
                if ( Ev == WM_LBUTTONUP && I->Self->OnClick ) I->Self->OnClick();
                else if ( Ev == WM_RBUTTONUP || Ev == WM_CONTEXTMENU ) I->Self->PopupMenu();
                return 0;
            }
            if ( I && M == I->TaskbarCreated && I->TaskbarCreated ) // explorer restarted: add the icon again
            {
                I->Added = Shell_NotifyIconW( NIM_ADD, &I->Nid ) != FALSE;
                return 0;
            }
            return DefWindowProcW( H, M, W, L );
        }
    };

    bool TrayIcon::Supported() { return true; }

    TrayIcon::TrayIcon( std::string Tooltip, const Bitmap* Icon ) : P( std::make_unique<Impl>() )
    {
        P->Self = this;
        WNDCLASSEXW Wc = {};
        Wc.cbSize = sizeof( Wc );
        Wc.lpfnWndProc = Impl::Proc;
        Wc.hInstance = GetModuleHandleW( nullptr );
        Wc.lpszClassName = TrayClass;
        RegisterClassExW( &Wc ); // fails harmlessly the second time
        P->Wnd = CreateWindowExW( 0, TrayClass, L"", 0, 0, 0, 0, 0, nullptr, nullptr, Wc.hInstance, nullptr );
        if ( !P->Wnd ) return;
        SetWindowLongPtrW( P->Wnd, GWLP_USERDATA, (LONG_PTR)P.get() );
        P->TaskbarCreated = RegisterWindowMessageW( L"TaskbarCreated" );
        NOTIFYICONDATAW& N = P->Nid;
        N.cbSize = sizeof( N );
        N.hWnd = P->Wnd;
        N.uID = 1;
        N.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        N.uCallbackMessage = TrayMsg;
        if ( Icon ) P->Own = MakeIcon( *Icon );
        N.hIcon = P->Own ? P->Own : LoadIconW( GetModuleHandleW( nullptr ), MAKEINTRESOURCEW( 1 ) );
        if ( !N.hIcon ) N.hIcon = LoadIconW( nullptr, MAKEINTRESOURCEW( 32512 ) ); // IDI_APPLICATION
        std::wstring T = os::Widen( Tooltip );
        wcsncpy( N.szTip, T.c_str(), sizeof( N.szTip ) / sizeof( wchar_t ) - 1 );
        P->Added = Shell_NotifyIconW( NIM_ADD, &N ) != FALSE;
    }

    TrayIcon::~TrayIcon()
    {
        if ( P->Added ) Shell_NotifyIconW( NIM_DELETE, &P->Nid );
        if ( P->Wnd ) DestroyWindow( P->Wnd );
        if ( P->Own ) DestroyIcon( P->Own );
    }

    bool TrayIcon::Ok() const { return P->Added; }

    void TrayIcon::SetTooltip( const std::string& Text )
    {
        std::wstring T = os::Widen( Text );
        std::fill( std::begin( P->Nid.szTip ), std::end( P->Nid.szTip ), L'\0' );
        wcsncpy( P->Nid.szTip, T.c_str(), sizeof( P->Nid.szTip ) / sizeof( wchar_t ) - 1 );
        P->Nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        if ( P->Added ) Shell_NotifyIconW( NIM_MODIFY, &P->Nid );
    }

    void TrayIcon::SetIcon( const Bitmap& Icon )
    {
        HICON I = MakeIcon( Icon );
        if ( !I ) return;
        if ( P->Own ) DestroyIcon( P->Own );
        P->Own = I;
        P->Nid.hIcon = I;
        P->Nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        if ( P->Added ) Shell_NotifyIconW( NIM_MODIFY, &P->Nid );
    }

    bool TrayIcon::Balloon( const std::string& Title, const std::string& Body )
    {
        if ( !P->Added ) return false;
        NOTIFYICONDATAW N = P->Nid;
        N.uFlags = NIF_INFO;
        std::wstring T = os::Widen( Title ), B = os::Widen( Body );
        wcsncpy( N.szInfoTitle, T.c_str(), sizeof( N.szInfoTitle ) / sizeof( wchar_t ) - 1 );
        wcsncpy( N.szInfo, B.c_str(), sizeof( N.szInfo ) / sizeof( wchar_t ) - 1 );
        N.dwInfoFlags = NIIF_INFO;
        return Shell_NotifyIconW( NIM_MODIFY, &N ) != FALSE;
    }

    void TrayIcon::PopupMenu()
    {
        if ( Items.empty() ) return;
        HMENU M = CreatePopupMenu();
        for ( size_t i = 0; i < Items.size(); i++ )
        {
            const Item& It = Items[ i ];
            if ( It.Text.empty() ) AppendMenuW( M, MF_SEPARATOR, 0, nullptr );
            else
                AppendMenuW( M, MF_STRING | ( It.Checked ? MF_CHECKED : 0 ) | ( It.Enabled ? 0 : MF_GRAYED ), UINT_PTR( i + 1 ),
                             os::Widen( It.Text ).c_str() );
        }
        POINT Pt;
        GetCursorPos( &Pt );
        SetForegroundWindow( P->Wnd ); // or the menu won't close when you click elsewhere
        UINT Cmd = UINT( TrackPopupMenu( M, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, Pt.x, Pt.y, 0, P->Wnd, nullptr ) );
        PostMessageW( P->Wnd, WM_NULL, 0, 0 );
        DestroyMenu( M );
        if ( Cmd >= 1 && Cmd <= Items.size() )
        {
            auto Fn = Items[ Cmd - 1 ].OnClick; // may replace Items
            if ( Fn ) Fn();
        }
    }

    namespace os
    {
        bool Notify( const std::string& Title, const std::string& Body )
        {
            // A balloon needs a tray icon: keep one (hidden from the tooltip, app icon) for the process.
            static std::unique_ptr<TrayIcon> Notifier;
            if ( !Notifier ) Notifier = std::make_unique<TrayIcon>( Title );
            return Notifier->Balloon( Title, Body );
        }
    }
}

#else
// ============================================================================ Linux / macOS (POSIX)

namespace tg::os
{
    // Run Argv (no shell), feed In to stdin, collect stdout. Returns the exit code (-1 = couldn't run).
    static int Run( const std::vector<std::string>& Argv, std::string* Out, const std::string* In = nullptr, bool Detach = false )
    {
        int OutP[ 2 ] = { -1, -1 }, InP[ 2 ] = { -1, -1 };
        if ( Out && pipe( OutP ) != 0 ) return -1;
        if ( In && pipe( InP ) != 0 ) return -1;
        posix_spawn_file_actions_t Fa;
        posix_spawn_file_actions_init( &Fa );
        if ( Out )
        {
            posix_spawn_file_actions_adddup2( &Fa, OutP[ 1 ], 1 );
            posix_spawn_file_actions_addclose( &Fa, OutP[ 0 ] );
        }
        else posix_spawn_file_actions_addopen( &Fa, 1, "/dev/null", O_WRONLY, 0 );
        if ( In )
        {
            posix_spawn_file_actions_adddup2( &Fa, InP[ 0 ], 0 );
            posix_spawn_file_actions_addclose( &Fa, InP[ 1 ] );
        }
        else posix_spawn_file_actions_addopen( &Fa, 0, "/dev/null", O_RDONLY, 0 );
        posix_spawn_file_actions_addopen( &Fa, 2, "/dev/null", O_WRONLY, 0 );
        std::vector<char*> A;
        for ( auto& S : Argv ) A.push_back( const_cast<char*>( S.c_str() ) );
        A.push_back( nullptr );
        pid_t Pid = 0;
        int Err = posix_spawnp( &Pid, A[ 0 ], &Fa, nullptr, A.data(), environ );
        posix_spawn_file_actions_destroy( &Fa );
        if ( Out ) close( OutP[ 1 ] );
        if ( In ) close( InP[ 0 ] );
        if ( Err != 0 )
        {
            if ( Out ) close( OutP[ 0 ] );
            if ( In ) close( InP[ 1 ] );
            return -1;
        }
        if ( In )
        {
            signal( SIGPIPE, SIG_IGN );
            size_t Done = 0;
            while ( Done < In->size() )
            {
                ssize_t N = write( InP[ 1 ], In->data() + Done, In->size() - Done );
                if ( N <= 0 ) break;
                Done += size_t( N );
            }
            close( InP[ 1 ] );
        }
        if ( Out )
        {
            char Buf[ 4096 ];
            ssize_t N;
            while ( ( N = read( OutP[ 0 ], Buf, sizeof Buf ) ) > 0 || ( N < 0 && errno == EINTR ) )
                if ( N > 0 ) Out->append( Buf, size_t( N ) );
            close( OutP[ 0 ] );
        }
        if ( Detach ) return 0; // e.g. xclip keeps serving the selection: don't wait for it
        int St = 0;
        while ( waitpid( Pid, &St, 0 ) < 0 && errno == EINTR ) {}
        return WIFEXITED( St ) ? WEXITSTATUS( St ) : -1;
    }

    static bool Have( const char* Tool )
    {
        const char* Path = std::getenv( "PATH" );
        if ( !Path ) return false;
        std::string P = Path;
        size_t B = 0;
        while ( B <= P.size() )
        {
            size_t E = P.find( ':', B );
            if ( E == std::string::npos ) E = P.size();
            std::string F = P.substr( B, E - B ) + "/" + Tool;
            if ( access( F.c_str(), X_OK ) == 0 ) return true;
            B = E + 1;
        }
        return false;
    }

    static std::vector<std::string> Lines( const std::string& S )
    {
        std::vector<std::string> Out;
        size_t B = 0;
        while ( B < S.size() )
        {
            size_t E = S.find( '\n', B );
            if ( E == std::string::npos ) E = S.size();
            if ( E > B ) Out.push_back( S.substr( B, E - B ) );
            B = E + 1;
        }
        return Out;
    }

#if defined( __APPLE__ )
    static std::string Quote( const std::string& S ) // AppleScript string literal
    {
        std::string O = "\"";
        for ( char C : S )
        {
            if ( C == '"' || C == '\\' ) O += '\\';
            O += C;
        }
        return O + "\"";
    }

    bool DialogsAvailable() { return true; }

    static std::vector<std::string> Script( const std::string& S )
    {
        std::string Out;
        if ( Run( { "osascript", "-e", S }, &Out ) != 0 ) return {};
        return Lines( Out );
    }

    std::vector<std::string> OpenFiles( const FileDialog& D )
    {
        std::string Ty;
        for ( auto& F : D.Filters )
            for ( auto& P : SplitPatterns( F.Patterns ) )
                if ( P.rfind( "*.", 0 ) == 0 && P != "*.*" ) Ty += ( Ty.empty() ? "" : ", " ) + Quote( P.substr( 2 ) );
        std::string Q = "choose file";
        if ( !D.Title.empty() ) Q += " with prompt " + Quote( D.Title );
        if ( !Ty.empty() ) Q += " of type {" + Ty + "}";
        if ( !D.Folder.empty() ) Q += " default location (POSIX file " + Quote( D.Folder ) + ")";
        if ( D.Multiple ) Q += " with multiple selections allowed";
        return Script( "set fs to (" + Q + ")\nif class of fs is not list then set fs to {fs}\nset o to \"\"\nrepeat with f in fs\nset o to o & POSIX path of f & linefeed\nend repeat\nreturn o" );
    }
    std::string SaveFile( const FileDialog& D )
    {
        std::string Q = "choose file name";
        if ( !D.Title.empty() ) Q += " with prompt " + Quote( D.Title );
        if ( !D.Name.empty() ) Q += " default name " + Quote( D.Name );
        if ( !D.Folder.empty() ) Q += " default location (POSIX file " + Quote( D.Folder ) + ")";
        auto R = Script( "POSIX path of (" + Q + ")" );
        return R.empty() ? std::string() : R[ 0 ];
    }
    std::string PickFolder( const FileDialog& D )
    {
        std::string Q = "choose folder";
        if ( !D.Title.empty() ) Q += " with prompt " + Quote( D.Title );
        auto R = Script( "POSIX path of (" + Q + ")" );
        return R.empty() ? std::string() : R[ 0 ];
    }
    bool Notify( const std::string& Title, const std::string& Body )
    {
        return Run( { "osascript", "-e", "display notification " + Quote( Body ) + " with title " + Quote( Title ) }, nullptr ) == 0;
    }
    bool OpenUrl( const std::string& U ) { return Run( { "open", U }, nullptr ) == 0; }
    bool GetClipboardImage( Bitmap& ) { return false; }
    bool SetClipboardImage( const Bitmap& ) { return false; }

#else // Linux / BSD
    enum class Kind { Open, Save, Folder };

    bool DialogsAvailable() { return Have( "zenity" ) || Have( "kdialog" ); }

    static std::vector<std::string> RunDialog( const FileDialog& D, Kind K )
    {
        std::string Out;
        std::string Start = D.Folder;
        if ( !Start.empty() && Start.back() != '/' ) Start += '/';
        if ( Have( "zenity" ) )
        {
            std::vector<std::string> A = { "zenity", "--file-selection" };
            if ( !D.Title.empty() ) A.push_back( "--title=" + D.Title );
            if ( K == Kind::Save ) { A.push_back( "--save" ); A.push_back( "--confirm-overwrite" ); }
            if ( K == Kind::Folder ) A.push_back( "--directory" );
            if ( K == Kind::Open && D.Multiple ) { A.push_back( "--multiple" ); A.push_back( "--separator=\n" ); }
            if ( !Start.empty() || !D.Name.empty() ) A.push_back( "--filename=" + Start + D.Name );
            if ( K != Kind::Folder )
                for ( auto& F : D.Filters )
                {
                    std::string Ps;
                    for ( auto& P : SplitPatterns( F.Patterns ) ) Ps += ( Ps.empty() ? "" : " " ) + P;
                    A.push_back( "--file-filter=" + F.Name + " | " + Ps );
                }
            if ( Run( A, &Out ) != 0 ) return {};
            return Lines( Out );
        }
        if ( Have( "kdialog" ) )
        {
            std::vector<std::string> A = { "kdialog" };
            if ( !D.Title.empty() ) { A.push_back( "--title" ); A.push_back( D.Title ); }
            A.push_back( K == Kind::Save ? "--getsavefilename" : K == Kind::Folder ? "--getexistingdirectory" : "--getopenfilename" );
            A.push_back( Start.empty() && D.Name.empty() ? "." : Start + D.Name );
            if ( K != Kind::Folder && !D.Filters.empty() )
            {
                std::string Fl;
                for ( auto& F : D.Filters )
                {
                    std::string Ps;
                    for ( auto& P : SplitPatterns( F.Patterns ) ) Ps += ( Ps.empty() ? "" : " " ) + P;
                    Fl += ( Fl.empty() ? "" : "|" ) + F.Name + " (" + Ps + ")";
                }
                A.push_back( Fl );
            }
            if ( K == Kind::Open && D.Multiple ) { A.push_back( "--multiple" ); A.push_back( "--separate-output" ); }
            if ( Run( A, &Out ) != 0 ) return {};
            return Lines( Out );
        }
        return {};
    }

    std::vector<std::string> OpenFiles( const FileDialog& D ) { return RunDialog( D, Kind::Open ); }
    std::string SaveFile( const FileDialog& D )
    {
        auto R = RunDialog( D, Kind::Save );
        return R.empty() ? std::string() : R[ 0 ];
    }
    std::string PickFolder( const FileDialog& D )
    {
        auto R = RunDialog( D, Kind::Folder );
        return R.empty() ? std::string() : R[ 0 ];
    }

    bool Notify( const std::string& Title, const std::string& Body )
    {
        if ( Have( "notify-send" ) ) return Run( { "notify-send", "--", Title, Body }, nullptr ) == 0;
        if ( Have( "gdbus" ) )
        {
            auto Q = []( const std::string& S ) { // GVariant string literal
                std::string O = "'";
                for ( char C : S )
                {
                    if ( C == '\'' || C == '\\' ) O += '\\';
                    O += C;
                }
                return O + "'";
            };
            return Run( { "gdbus", "call", "--session", "--dest", "org.freedesktop.Notifications", "--object-path", "/org/freedesktop/Notifications",
                          "--method", "org.freedesktop.Notifications.Notify", "tg", "0", "", Q( Title ), Q( Body ), "[]", "{}", "5000" },
                        nullptr ) == 0;
        }
        return false;
    }

    bool OpenUrl( const std::string& U ) { return Have( "xdg-open" ) && Run( { "xdg-open", U }, nullptr, nullptr, true ) == 0; }

    static bool Wayland() { return std::getenv( "WAYLAND_DISPLAY" ) != nullptr; }

    bool GetClipboardImage( Bitmap& Out )
    {
        std::string Data;
        int Rc = -1;
        if ( Wayland() && Have( "wl-paste" ) ) Rc = Run( { "wl-paste", "--no-newline", "--type", "image/png" }, &Data );
        else if ( Have( "xclip" ) ) Rc = Run( { "xclip", "-selection", "clipboard", "-t", "image/png", "-o" }, &Data );
        if ( Rc != 0 || Data.empty() ) return false;
        int W = 0, H = 0, N = 0;
        unsigned char* Px = stbi_load_from_memory( (const unsigned char*)Data.data(), int( Data.size() ), &W, &H, &N, 4 );
        if ( !Px ) return false;
        Out.Width = W;
        Out.Height = H;
        Out.Pixels.resize( size_t( W ) * H );
        std::memcpy( Out.Pixels.data(), Px, Out.Pixels.size() * 4 );
        stbi_image_free( Px );
        return true;
    }

    bool SetClipboardImage( const Bitmap& Img )
    {
        if ( Img.Empty() ) return false;
        std::string Png;
        stbi_write_png_to_func( []( void* Ctx, void* D, int N ) { ( (std::string*)Ctx )->append( (const char*)D, size_t( N ) ); }, &Png, Img.Width,
                                Img.Height, 4, Img.Pixels.data(), Img.Width * 4 );
        if ( Png.empty() ) return false;
        // both tools fork into the background to keep serving the clipboard
        if ( Wayland() && Have( "wl-copy" ) ) return Run( { "wl-copy", "--type", "image/png" }, nullptr, &Png ) == 0;
        if ( Have( "xclip" ) ) return Run( { "xclip", "-selection", "clipboard", "-t", "image/png", "-i" }, nullptr, &Png ) == 0;
        return false;
    }
#endif
}

namespace tg
{
    struct TrayIcon::Impl {};
    TrayIcon::TrayIcon( std::string, const Bitmap* ) : P( std::make_unique<Impl>() ) {}
    TrayIcon::~TrayIcon() = default;
    bool TrayIcon::Supported() { return false; }
    bool TrayIcon::Ok() const { return false; }
    void TrayIcon::SetTooltip( const std::string& ) {}
    void TrayIcon::SetIcon( const Bitmap& ) {}
    bool TrayIcon::Balloon( const std::string& Title, const std::string& Body ) { return os::Notify( Title, Body ); }
    void TrayIcon::PopupMenu() {}
}
#endif
