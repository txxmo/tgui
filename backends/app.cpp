// tg::App on GLFW + OpenGL 3. Swap this file to put App on another platform/renderer.
#include "tg/app.h"
#include "app_common.h"
#include "gl3_renderer.h"
#include "glfw_platform.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <vector>

namespace tg
{
    // A second (third, ...) OS window: shares the main window's GL objects and fonts.
    struct GlfwWindow final : OsWindow
    {
        GLFWwindow* Win = nullptr;
        GL3Renderer Gl;
        std::unique_ptr<Context> Ctx;
        Container* MainArea = nullptr;
        Widget* WindowLayer = nullptr;
        bool Closing = false;
        detail::PoppedOut Popped;

        Container& Main() override { return *MainArea; }
        Context& GetContext() override { return *Ctx; }
        void SetTitle( const std::string& T ) override { if ( Win ) glfwSetWindowTitle( Win, T.c_str() ); }
        void Close() override { Closing = true; }
        bool IsOpen() const override { return !Closing && Win && !glfwWindowShouldClose( Win ); }
    };

    struct App::Impl
    {
        AppConfig Cfg;
        GLFWwindow* Win = nullptr;
        GL3Renderer Gl;
        std::unique_ptr<Context> Ctx;
        Container* MainArea = nullptr;
        Widget* WindowLayer = nullptr;
        int Cascade = 0;
        bool Good = false;
        std::vector<std::unique_ptr<GlfwWindow>> Extra;

        void DrawWindow( GLFWwindow* W, Context& C )
        {
            glfwMakeContextCurrent( W );
            glfw::Sync( W, C );
            if ( !C.NeedsFrame() ) return;
            C.Frame();
            int Fw, Fh;
            glfwGetFramebufferSize( W, &Fw, &Fh );
            if ( Fw <= 0 || Fh <= 0 ) return;
            C.Render( { float( Fw ), float( Fh ) } );
            glfwSwapBuffers( W );
        }

        void Reap() // windows closed this iteration: put popped-out widgets back, then destroy
        {
            for ( size_t i = 0; i < Extra.size(); )
            {
                GlfwWindow& X = *Extra[ i ];
                if ( !X.Closing && X.Win && !glfwWindowShouldClose( X.Win ) ) { i++; continue; }
                X.Closing = true;
                if ( X.OnClose ) X.OnClose();
                detail::PutBack( X.Popped, *X.MainArea );
                Ctx->RequestFrame();
                if ( X.Win )
                {
                    glfw::Uninstall( X.Win );
                    glfwMakeContextCurrent( X.Win );
                }
                X.Ctx.reset();
                if ( X.Win )
                {
                    X.Gl.Shutdown();
                    glfwDestroyWindow( X.Win );
                }
                Extra.erase( Extra.begin() + long( i ) );
                glfwMakeContextCurrent( Win );
            }
        }
    };

    App::App( const std::string& Title, int W, int H ) : App( AppConfig{ Title, W, H } ) {}

    App::App( const AppConfig& Cfg ) : P( std::make_unique<Impl>() )
    {
        P->Cfg = Cfg;
#if defined( TG_GLFW_X11 ) && GLFW_VERSION_MAJOR * 100 + GLFW_VERSION_MINOR >= 304
        // WebView on Linux embeds GTK through X11 (XWayland on a Wayland desktop)
        if ( glfwPlatformSupported( GLFW_PLATFORM_X11 ) ) glfwInitHint( GLFW_PLATFORM, GLFW_PLATFORM_X11 );
#endif
        if ( !glfwInit() ) { std::fprintf( stderr, "tg: glfwInit failed\n" ); return; }
        glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
        glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 3 );
        glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
        glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, 1 );
        glfwWindowHint( GLFW_SCALE_TO_MONITOR, 1 );
        P->Win = glfwCreateWindow( Cfg.Width, Cfg.Height, Cfg.Title.c_str(), nullptr, nullptr );
        if ( !P->Win ) { std::fprintf( stderr, "tg: couldn't create a GL 3.3 window\n" ); return; }
        glfwMakeContextCurrent( P->Win );
        glfwSwapInterval( Cfg.VSync ? 1 : 0 );
        if ( !P->Gl.Init( []( const char* N ) { return (void*)glfwGetProcAddress( N ); } ) ) return;

        if ( Cfg.Light ) GetTheme().UseColors( Theme::Light() );
        P->Ctx = std::make_unique<Context>( P->Gl );
        float Scale = 1, Dummy;
        glfwGetWindowContentScale( P->Win, &Scale, &Dummy );
        if ( !P->Ctx->LoadDefaultFonts( Scale > 0 ? Scale : 1 ) ) return;
        glfw::Install( P->Win, *P->Ctx );
        detail::BuildRoot( *P->Ctx, P->MainArea, P->WindowLayer );
        P->Good = true;
    }

    App::~App()
    {
        for ( auto& X : P->Extra ) // popped-out widgets go home first (their owners live in the main tree)
        {
            detail::PutBack( X->Popped, *X->MainArea );
            if ( !X->Win ) continue;
            glfw::Uninstall( X->Win );
            glfwMakeContextCurrent( X->Win );
            X->Ctx.reset();
            X->Gl.Shutdown();
            glfwDestroyWindow( X->Win );
        }
        P->Extra.clear();
        if ( P->Win ) glfwMakeContextCurrent( P->Win );
        P->Ctx.reset(); // widgets + font texture go before the GL context
        P->Gl.Shutdown();
        if ( P->Win ) glfwDestroyWindow( P->Win );
        glfwTerminate();
    }

    bool App::Ok() const { return P->Good; }
    Container& App::Main() { return *P->MainArea; }
    Context& App::GetContext() { return *P->Ctx; }
    Renderer& App::GetRenderer() { return P->Gl; }
    void App::SetContinuous( bool On ) { P->Cfg.Continuous = On; }
    void App::Quit() { if ( P->Win ) glfwSetWindowShouldClose( P->Win, 1 ); }
    size_t App::WindowCount() const { return 1 + P->Extra.size(); }

    tg::Window& App::Window( std::string Title, Vec2 Pos, Vec2 Size )
    {
        if ( Pos.x < 0 || Pos.y < 0 )
        {
            Pos = { 40.f + 32 * ( P->Cascade % 8 ), 40.f + 32 * ( P->Cascade % 8 ) };
            P->Cascade++;
        }
        return P->WindowLayer->Add<tg::Window>( std::move( Title ), Pos, Size );
    }

    OsWindow& App::NewWindow( std::string Title, int Width, int Height )
    {
        auto X = std::make_unique<GlfwWindow>();
        // share = the main window: textures and buffers are visible to both GL contexts
        X->Win = glfwCreateWindow( Width, Height, Title.c_str(), nullptr, P->Win );
        if ( X->Win )
        {
            glfwMakeContextCurrent( X->Win );
            glfwSwapInterval( 0 ); // only the main window waits for vblank: two waits would halve the frame rate
            X->Gl.Init( []( const char* N ) { return (void*)glfwGetProcAddress( N ); } );
            X->Ctx = std::make_unique<Context>( X->Gl, *P->Ctx );
            glfw::Install( X->Win, *X->Ctx );
            glfwMakeContextCurrent( P->Win );
        }
        else // couldn't open a window: keep the API usable, nothing shows
        {
            X->Ctx = std::make_unique<Context>( P->Gl, *P->Ctx );
            X->Closing = true;
        }
        detail::BuildRoot( *X->Ctx, X->MainArea, X->WindowLayer );
        P->Extra.push_back( std::move( X ) );
        return *P->Extra.back();
    }

    OsWindow& App::PopOut( Widget& W, std::string Title, int Width, int Height )
    {
        Vec2 S = detail::PopOutSize( W, Width, Height );
        auto& X = static_cast<GlfwWindow&>( NewWindow( std::move( Title ), int( S.x ), int( S.y ) ) );
        X.Popped = detail::PopOutInto( W, *X.MainArea );
        P->Ctx->RequestFrame();
        return X;
    }

    int App::Run()
    {
        if ( !P->Good ) return 1;
        Context& Ctx = *P->Ctx;
        while ( !glfwWindowShouldClose( P->Win ) )
        {
            if ( P->Cfg.Continuous ) Ctx.RequestFrame();
            // sleep until any window has something to do
            double T = Ctx.SecondsUntilNextFrame();
            for ( auto& X : P->Extra ) T = std::min( T, X->Ctx->SecondsUntilNextFrame() );
            if ( T <= 0 ) glfwPollEvents();
            else glfwWaitEventsTimeout( std::min( T, 0.5 ) );
            Ctx.RunPumps();
            for ( auto& X : P->Extra ) X->Ctx->RunPumps();
            P->Reap();

            if ( OnUpdate ) OnUpdate();
            P->DrawWindow( P->Win, Ctx );
            for ( auto& X : P->Extra )
                if ( X->Win ) P->DrawWindow( X->Win, *X->Ctx );
            glfwMakeContextCurrent( P->Win );
        }
        return 0;
    }
}
