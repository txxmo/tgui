// tg::Pty — run a program inside a pseudo-terminal (forkpty on Linux/macOS, ConPTY on Windows 10 1809+).
// Usually you don't use this directly: Terminal::Spawn() does. Use it to drive a shell without a widget.
//
//   tg::Pty P;
//   P.Start( { "bash" }, []( std::string Out ) { /* reader thread! */ }, []( int Code ) { /* exited */ } );
//   P.Write( "ls\r" );
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tg
{
    struct PtyOptions
    {
        std::string Command;   // empty = the user's shell ($SHELL / cmd.exe). Run through the shell, so "ssh me@vps" works.
        std::string Cwd;       // empty = current directory
        int Cols = 80, Rows = 24;
        std::vector<std::pair<std::string, std::string>> Env; // extra variables (TERM=xterm-256color is set for you)
    };

    class Pty
    {
    public:
        Pty();
        ~Pty();                // kills the process and joins the reader thread
        Pty( const Pty& ) = delete;
        Pty& operator=( const Pty& ) = delete;

        // OnOutput / OnExit run on the Pty's reader thread — hop to the UI thread with Context::Post.
        bool Start( const PtyOptions& Options, std::function<void( std::string )> OnOutput, std::function<void( int ExitCode )> OnExit = {} );
        void Write( std::string_view Bytes );
        void Resize( int Cols, int Rows );
        void Kill();
        bool Running() const;
        static bool Supported(); // false on Windows before 10 1809

    private:
        struct Impl;
        std::unique_ptr<Impl> P;
    };
}
