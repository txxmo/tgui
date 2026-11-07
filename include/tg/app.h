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

        std::function<void()> OnUpdate; // runs on the UI thread each loop iteration, before drawing
        void SetContinuous( bool On );
        void Quit();
        int Run();

    private:
        struct Impl;
        std::unique_ptr<Impl> P;
    };
}
