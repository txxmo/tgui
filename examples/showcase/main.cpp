// Showcase: the full demo app on tg::App (GLFW + OpenGL 3).
// Background threads feed "live" data through Ctx.Post(), like a real network/stats/decoder thread would.
#include "../common/demo_ui.h"
#include "tg/app.h"
#include <atomic>
#include <chrono>
#include <thread>

#include <cstdlib>
#include <cstring>

int main( int Argc, char** Argv )
{
    tg::App App( "tg showcase", 1280, 820 );
    if ( !App.Ok() ) return 1;
    tg::Context& Ctx = App.GetContext();

    DemoApp Demo;
    // Desktop tab: "Pop out notes" moves a docked panel into its own OS window and back
    Demo.PopOut = [ & ]( tg::DockPanel& P ) {
        Demo.DockW->Close( P );
        auto& W = App.PopOut( P, "Notes", 520, 420 );
        tg::DockPanel* Pp = &P;
        W.OnClose = [ &Demo, &Ctx, Pp ] { Ctx.Post( [ &Demo, Pp ] { Demo.DockW->Show( *Pp ); } ); }; // after it's back home
    };
    Demo.Build( Ctx, App.Main() );

    // tray icon with a menu (Windows; elsewhere TrayIcon::Supported() is false and this does nothing)
    tg::TrayIcon Tray( "tg showcase" );
    Tray.Items = { { "Desktop tab", [ & ] { Demo.Tabs->Select( 12 ); } },
                   { "Send a notification", [ & ] { Tray.Balloon( "tg showcase", "Hello from the tray" ); } },
                   {},
                   { "Quit", [ & ] { App.Quit(); } } };
    Tray.OnClick = [ & ] { Demo.Tabs->Select( 12 ); };
    for ( int i = 1; i + 1 < Argc; i++ ) // tg_showcase --tab 11   (open on a tab)
        if ( !std::strcmp( Argv[ i ], "--tab" ) ) Demo.Tabs->Select( std::atoi( Argv[ i + 1 ] ) );

    // Simulated producers. They never touch widgets directly, only Post().
    std::atomic<bool> Running = true;
    std::thread Stats( [ & ] {
        for ( double t = 0; Running; t += 1 )
        {
            Ctx.Post( [ &Demo, t ] { Demo.TickStats( t ); } );
            for ( int k = 0; k < 10 && Running; k++ ) std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
        }
    } );
    std::thread Video( [ & ] {
        auto T0 = std::chrono::steady_clock::now();
        while ( Running )
        {
            double t = std::chrono::duration<double>( std::chrono::steady_clock::now() - T0 ).count();
            Ctx.Post( [ &Demo, t ] { if ( Demo.Page == 1 ) Demo.TickScreenShare( t ); } ); // only "decode" while visible
            std::this_thread::sleep_for( std::chrono::milliseconds( 33 ) );
        }
    } );
    std::thread Chat( [ & ] {
        static const char* Lines[] = { "deploy finished, all green", "anyone up for reviewing the auth PR?", "lol the graph", "brb coffee" };
        static const char* Who[] = { "mo", "rika", "alex", "mo" };
        for ( int i = 0; Running; i++ )
        {
            for ( int k = 0; k < 70 && Running; k++ ) std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
            if ( Running ) Ctx.Post( [ &Demo, i ] { Demo.AddMessage( Who[ i % 4 ], Lines[ i % 4 ], "now" ); } );
        }
    } );

    int Code = App.Run();
    Running = false;
    Stats.join();
    Video.join();
    Chat.join();
    return Code;
}
