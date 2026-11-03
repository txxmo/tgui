// Showcase: GLFW window + OpenGL 3 renderer + the demo app.
// Background threads feed "live" data through Ctx.Post(), like a real network/stats/decoder thread would.
#include "../common/demo_ui.h"
#include "gl3_renderer.h"
#include "glfw_platform.h"
#include <GLFW/glfw3.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

#ifndef TG_ASSET_DIR
#define TG_ASSET_DIR "assets"
#endif

int main()
{
    if ( !glfwInit() ) return 1;
    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 3 );
    glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
    glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, 1 );
    glfwWindowHint( GLFW_SCALE_TO_MONITOR, 1 );
    GLFWwindow* Win = glfwCreateWindow( 1280, 800, "tg showcase", nullptr, nullptr );
    if ( !Win ) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent( Win );
    glfwSwapInterval( 1 );

    tg::GL3Renderer Gl;
    if ( !Gl.Init( []( const char* N ) { return (void*)glfwGetProcAddress( N ); } ) ) return 1;

    tg::Context Ctx( Gl );
    float Scale = 1, Dummy;
    glfwGetWindowContentScale( Win, &Scale, &Dummy );
    if ( !Ctx.LoadDefaultFonts( TG_ASSET_DIR "/fonts", Scale ) ) { std::fprintf( stderr, "couldn't load fonts from %s\n", TG_ASSET_DIR ); return 1; }
    tg::glfw::Install( Win, Ctx );

    DemoApp App;
    App.Build( Ctx );

    // Simulated background producers. Note: they never touch widgets directly, only Post().
    std::atomic<bool> Running = true;
    std::thread Stats( [ & ] {
        for ( double t = 0; Running; t += 1 ) { Ctx.Post( [ &App, t ] { App.TickStats( t ); } ); std::this_thread::sleep_for( std::chrono::seconds( 1 ) ); }
    } );
    std::thread Video( [ & ] {
        auto T0 = std::chrono::steady_clock::now();
        while ( Running )
        {
            double t = std::chrono::duration<double>( std::chrono::steady_clock::now() - T0 ).count();
            Ctx.Post( [ &App, t ] { App.TickScreenShare( t ); } );
            std::this_thread::sleep_for( std::chrono::milliseconds( 33 ) ); // ~30 fps "stream"
        }
    } );
    std::thread Chat( [ & ] {
        const char* Lines[] = { "deploy finished, all green", "anyone up for reviewing the auth PR?", "lol the graph", "brb coffee" };
        const char* Who[] = { "mo", "rika", "alex", "mo" };
        for ( int i = 0; Running; i++ )
        {
            for ( int k = 0; k < 70 && Running; k++ ) std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
            if ( Running ) Ctx.Post( [ &App, i, &Lines, &Who ] { App.AddMessage( Who[ i % 4 ], Lines[ i % 4 ], "now" ); } );
        }
    } );

    while ( !glfwWindowShouldClose( Win ) )
    {
        tg::glfw::WaitForWork( Ctx ); // sleeps when nothing changes
        tg::glfw::Sync( Win, Ctx );
        if ( !Ctx.NeedsFrame() ) continue;

        Ctx.Frame();
        int Fw, Fh;
        glfwGetFramebufferSize( Win, &Fw, &Fh );
        Ctx.Render( { float( Fw ), float( Fh ) } );
        glfwSwapBuffers( Win );
    }

    Running = false;
    Stats.join();
    Video.join();
    Chat.join();
    Gl.Shutdown();
    glfwDestroyWindow( Win );
    glfwTerminate();
    return 0;
}
