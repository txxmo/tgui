// tg::App — window + GPU + event loop in one object. Link tg::app (CMake) to use it.
//
//   #include "tg/app.h"
//   int main()
//   {
//       tg::App App( "Hello" );
//       static bool Enabled = true;
//       auto& W = App.Window( "Settings" );
//       W.Checkbox( "Enabled", &Enabled );
//       return App.Run();
//   }
#pragma once
#include "tg.h"
#include <functional>
#include <memory>
#include <string>

namespace tg
{
    struct AppConfig
    {
        std::string Title = "tg";
        int Width = 1280, Height = 800;
        bool VSync = true;
        bool Continuous = false; // redraw every frame (games/overlays). Default: only when something changes.
        bool Light = false;      // start with the light palette
    };

    // Another OS window of the same App: its own widget tree and Context, sharing fonts and theme.
    class OsWindow
    {
    public:
        virtual ~OsWindow() = default;
        virtual Container& Main() = 0;
        virtual Context& GetContext() = 0;
        virtual void SetTitle( const std::string& Title ) = 0;
        virtual void Close() = 0;          // closes it at the end of this loop iteration
        virtual bool IsOpen() const = 0;
        std::function<void()> OnClose;     // the user (or Close()) closed it; the object goes away right after
    };

    class App
    {
    public:
        explicit App( const std::string& Title = "tg", int Width = 1280, int Height = 800 );
        explicit App( const AppConfig& Config );
        ~App();
        App( const App& ) = delete;
        App& operator=( const App& ) = delete;

        bool Ok() const; // false if the window/GL context couldn't be created

        // Full-window area behind the floating windows. Put a whole app layout here,
        // or ignore it and only use Window().
        Container& Main();

        // A floating window. Pos {-1,-1} = cascade automatically. Size.y = 0 = fit content.
        tg::Window& Window( std::string Title, Vec2 Pos = { -1, -1 }, Vec2 Size = { 340, 0 } );

        Context& GetContext();
        Renderer& GetRenderer();

        // More OS windows. Each has its own Main() area; widgets, popups and windows work as usual inside.
        //   auto& W = App.NewWindow( "Inspector", 420, 600 );
        //   W.Main().Heading( "Inspector" );
        OsWindow& NewWindow( std::string Title, int Width = 640, int Height = 480 );
        // Move W (with everything under it) into its own OS window; closing that window puts it back
        // where it was. Width/Height 0 = W's current size.
        OsWindow& PopOut( Widget& W, std::string Title, int Width = 0, int Height = 0 );
        size_t WindowCount() const; // open OS windows, the main one included

        std::function<void()> OnUpdate; // runs on the UI thread each loop iteration, before drawing
        void SetContinuous( bool On );
        void Quit();
        int Run();

        struct Impl; // (backend internals)

    private:
        std::unique_ptr<Impl> P;
    };
}
