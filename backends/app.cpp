// tg::App on GLFW + OpenGL 3. Swap this file to put App on another platform/renderer.
#include "tg/app.h"
#include "gl3_renderer.h"
#include "glfw_platform.h"
#include <GLFW/glfw3.h>
#include <cstdio>

namespace tg
{
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
    };

    App::App( const std::string& Title, int W, int H ) : App( AppConfig{ Title, W, H } ) {}

    App::App( const AppConfig& Cfg ) : P( std::make_unique<Impl>() )
    {
        P->Cfg = Cfg;
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

        // Root = Stack[ main area, floating window layer ]
        Widget& Root = P->Ctx->Root();
        Root.Layout.Direction = Dir::Stack;
        P->MainArea = &Root.Add<Container>( Dir::Column, 0 );
        P->WindowLayer = &Root.Add<Widget>();
        P->WindowLayer->Layout.Direction = Dir::Free;
        P->Good = true;
    }

    App::~App()
    {
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

    tg::Window& App::Window( std::string Title, Vec2 Pos, Vec2 Size )
    {
        if ( Pos.x < 0 || Pos.y < 0 )
        {
            Pos = { 40.f + 32 * ( P->Cascade % 8 ), 40.f + 32 * ( P->Cascade % 8 ) };
            P->Cascade++;
        }
        return P->WindowLayer->Add<tg::Window>( std::move( Title ), Pos, Size );
    }

    int App::Run()
    {
        if ( !P->Good ) return 1;
        Context& Ctx = *P->Ctx;
        while ( !glfwWindowShouldClose( P->Win ) )
        {
            if ( P->Cfg.Continuous ) Ctx.RequestFrame();
            glfw::WaitForWork( Ctx );
            glfw::Sync( P->Win, Ctx );
            if ( OnUpdate ) OnUpdate();
            if ( !Ctx.NeedsFrame() ) continue;

            Ctx.Frame();
            int Fw, Fh;
            glfwGetFramebufferSize( P->Win, &Fw, &Fh );
            Ctx.Render( { float( Fw ), float( Fh ) } );
            glfwSwapBuffers( P->Win );
        }
        return 0;
    }
}
