// tg/context.h — owns the widget tree, routes input, runs layout, produces the DrawList.
//
// Typical loop (see examples/glfw_gl3):
//     ctx.OnMouseMove(...) / OnMouseButton(...) / OnKey(...)   <- from your platform layer
//     if ( ctx.NeedsFrame() ) { ctx.Frame(); ctx.Render( fbSize ); swap(); }
//     else wait for events with timeout ctx.SecondsUntilNextFrame()
#pragma once
#include "draw.h"
#include "font.h"
#include "input.h"
#include "widget.h"
#include <functional>
#include <memory>
#include <string>

namespace tg
{
    class Renderer;

    class Context
    {
    public:
        explicit Context( Renderer& R );
        ~Context();

        Renderer& GetRenderer() { return R; }
        FontAtlas Fonts;

        // Loads Inter (assets/fonts) at the usual sizes, builds the atlas, wires up the theme fonts.
        // Scale = DPI scale so text is rasterized crisply on hi-dpi screens.
        bool LoadDefaultFonts( const std::string& FontDir, float Scale = 1.f );

        Widget& Root() { return *RootWidget; }

        // ---- feed input (UI units, not framebuffer pixels) ----
        void SetViewport( Vec2 Size, float Scale = 1.f ); // Size in UI units, Scale = framebuffer px per unit
        Vec2 GetViewport() const { return Viewport; }
        void OnMouseMove( Vec2 Pos );
        void OnMouseButton( MouseButton B, bool Down );
        void OnMouseWheel( Vec2 Delta );
        void OnKey( Key K, bool Down, uint8_t Mods, bool Repeat = false );
        void OnText( uint32_t Codepoint );

        // ---- frame ----
        bool NeedsFrame() const;
        double SecondsUntilNextFrame() const; // how long the platform may sleep (inf-ish if idle)
        const DrawList& Frame();              // run posted tasks, layout (if dirty), draw
        void Render( Vec2 FramebufferSize ); // hand the last frame to the renderer

        // ---- for widgets ----
        void RequestFrame() { FrameRequested = true; }
        void RequestFrameIn( double Seconds ); // timed redraw, e.g. caret blink
        void InvalidateLayout() { LayoutDirty = true; FrameRequested = true; }
        float DeltaTime() const { return Dt; }
        // Tests/recording: advance time by a fixed step per Frame() instead of the wall clock. 0 = off.
        void SetFixedTimestep( float Seconds ) { FixedDt = Seconds; }
        double Time() const { return Now; }
        Vec2 MousePos() const { return Mouse; }
        Widget* Hovered() const { return HoverW; }
        Widget* Pressed() const { return PressW; }
        Widget* Focused() const { return FocusW; }
        void SetFocus( Widget* W );
        Cursor GetCursor() const;

        // Called by the tree when a widget is destroyed so we never keep dangling pointers.
        void Forget( Widget* W );

        // ---- app hooks ----
        std::function<void( const std::string& )> SetClipboard;
        std::function<std::string()> GetClipboard;
        std::function<void()> WakeUp; // platform: interrupt a blocking event wait (glfwPostEmptyEvent)

        // Run Fn on the UI thread at the start of the next frame. Safe to call from any thread
        // (network thread receiving chat messages, stats poller, decoder thread, ...).
        void Post( std::function<void()> Fn );

    private:
        Renderer& R;
        std::unique_ptr<Widget> RootWidget;
        DrawList List;
        Vec2 Viewport, Mouse;
        float Scale = 1;
        float Dt = 0, FixedDt = 0;
        double Now = 0;
        double NextTimedFrame = -1;
        double Start;
        double Clock() const; // seconds since the context was created
        bool LayoutDirty = true, FrameRequested = true;
        Widget *HoverW = nullptr, *PressW = nullptr, *FocusW = nullptr;

        struct PostQueue;
        std::unique_ptr<PostQueue> Posts;

        void UpdateHover();
        bool Dispatch( Widget* Target, const Event& E ); // bubbles up
    };
}
