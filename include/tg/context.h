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
#include <utility>
#include <vector>

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

        // Loads Inter at the usual sizes, builds the atlas, wires up the theme fonts.
        // No arguments = the copy compiled into the library (nothing to ship next to your exe).
        // Scale = DPI scale so text is rasterized crisply on hi-dpi screens.
        bool LoadDefaultFonts( float Scale = 1.f );
        bool LoadDefaultFonts( const std::string& FontDir, float Scale );
        // Your own font files (TTF/OTF): Regular for body text, Bold for headings/titles, Medium optional
        // (falls back to Regular). Example: LoadFonts( "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/segoeuib.ttf", Scale )
        bool LoadFonts( const std::string& Regular, const std::string& Bold, float Scale = 1.f, const std::string& Medium = {} );

        Widget& Root() { return *RootWidget; }
        Widget& Overlay() { return *OverlayWidget; } // popups live here, above everything
        void ClosePopups();                          // close every open popup
        bool HasPopups() const;
        // Deepest visible widget under P (interactive or not), overlay first. Used for tooltips / context menus.
        Widget* DeepestAt( Vec2 P );
        float TooltipDelay = 0.45f;                  // seconds of hovering before a tooltip shows
        Widget* ShownTooltip() const { return TipW && !TipSuppressed && Now - TipSince >= TooltipDelay ? TipW : nullptr; }

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
        float GetScale() const { return Scale; } // device pixels per UI unit
        // Tests/recording: advance time by a fixed step per Frame() instead of the wall clock. 0 = off.
        void SetFixedTimestep( float Seconds ) { FixedDt = Seconds; }
        double Time() const { return Now; }
        Vec2 MousePos() const { return Mouse; }
        Widget* Hovered() const { return HoverW; }
        Widget* Pressed() const { return PressW; }
        Widget* Focused() const { return FocusW; }
        void SetFocus( Widget* W );
        Cursor GetCursor() const;

        // ---- keyboard / mouse-button state (for your app: poll binds, hotkeys) ----
        bool IsKeyDown( Key K ) const { return KeyState[ size_t( K ) ]; }
        uint8_t Mods() const; // modifier keys held right now (ModShift | ModCtrl ...); also set on mouse events
        float DoubleClickTime = 0.4f;
        uint32_t KeyPressCount( Key K ) const { return PressCount[ size_t( K ) ]; } // rising edges since start
        // Sees every key / mouse-button press before widgets do (not while a Keybind is listening,
        // and keyboard keys not while a text field has focus).
        // Return true to swallow it. Example: toggle a menu with Insert, like the old tgui.
        std::function<bool( Key K, bool Down )> OnKeyEvent;

        // Route every mouse button + key event to W until released (a Keybind waiting for a key).
        void Capture( Widget* W ) { CaptureW = W; RequestFrame(); }
        void ReleaseCapture( Widget* W ) { if ( CaptureW == W ) CaptureW = nullptr; RequestFrame(); }
        Widget* Captured() const { return CaptureW; }

        // Called by the tree when a widget is destroyed so we never keep dangling pointers.
        void Forget( Widget* W );

        // ---- app hooks ----
        std::function<void( const std::string& )> SetClipboard;
        std::function<std::string()> GetClipboard;
        std::function<void()> WakeUp; // platform: interrupt a blocking event wait (glfwPostEmptyEvent)

        // Run Fn on the UI thread at the start of the next frame. Safe to call from any thread
        // (network thread receiving chat messages, stats poller, decoder thread, ...).
        void Post( std::function<void()> Fn );

        // ---- native integration (WebView and other native child windows)
        void* NativeWindow = nullptr; // HWND / X11 Window / NSWindow* we draw into; set by the platform layer
        uint64_t FrameIndex() const { return Frames; }
        // Fn runs at the end of every Frame(): native children follow their widget's bounds here.
        void AddFrameHook( const void* Key, std::function<void()> Fn );
        void RemoveFrameHook( const void* Key );
        // Fn runs every loop iteration, at least every 10 ms while registered (GTK's event loop for WebKit).
        void AddPump( const void* Key, std::function<void()> Fn );
        void RemovePump( const void* Key );
        void RunPumps();

    private:
        Renderer& R;
        std::unique_ptr<Widget> RootWidget, OverlayWidget;
        DrawList List;
        Vec2 Viewport, Mouse;
        float Scale = 1;
        float Dt = 0, FixedDt = 0;
        double Now = 0;
        double NextTimedFrame = -1;
        double Start;
        double Clock() const; // seconds since the context was created
        bool LayoutDirty = true, FrameRequested = true;
        Widget *HoverW = nullptr, *PressW = nullptr, *FocusW = nullptr, *CaptureW = nullptr;
        bool KeyState[ size_t( Key::Count ) ] = {};
        uint32_t PressCount[ size_t( Key::Count ) ] = {};
        bool TrackKey( Key K, bool Down ); // updates state; returns true if OnKeyEvent swallowed it

        // multi-click detection
        double LastClickTime = -1;
        Vec2 LastClickPos;
        MouseButton LastClickButton = MouseButton::Left;
        uint8_t ClickCount = 0;

        // tooltip state
        Widget* TipW = nullptr;
        double TipSince = 0;
        bool TipSuppressed = false;
        void UpdateTooltip();
        void DrawTooltip();
        bool OpenContextMenu();

        uint64_t Frames = 0;
        std::vector<std::pair<const void*, std::function<void()>>> FrameHooks, Pumps;

        struct PostQueue;
        std::unique_ptr<PostQueue> Posts;

        void UpdateHover();
        Widget* HitTestAll( Vec2 P );
        bool Dispatch( Widget* Target, const Event& E ); // bubbles up
    };
}
