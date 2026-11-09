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
#include "binds.h"
#include "history.h"
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

        // Key binds on controls (right-click a control -> "Add key bind"). See tg/binds.h.
        KeyBinds Binds{ *this };
        // Undo / redo of control edits (Ctrl+Z / Ctrl+Y). See tg/history.h.
        tg::History History{ *this };

        // ---- keyboard navigation: Tab / Shift+Tab move focus, Enter / Space click, arrows adjust;
        //      arrows + Enter in open menus and dropdowns. A focus ring shows while you use the keyboard.
        bool TabNavigation = true;
        void FocusNext( bool Backward = false );
        bool FocusRingVisible() const { return FocusRing; }
        Widget* KeyHot() const { return KeyHotW; } // popup item highlighted with the arrow keys
        void Click( Widget& W );                   // a full click (down + up) on W, as if with the mouse

        // ---- shortcuts: app-wide key combos. With Ctrl/Alt/Super they work everywhere; plain keys
        //      don't fire while a text field has focus.   Ctx.Shortcut( "Ctrl+S", [] { Save(); } );
        uint32_t Shortcut( const std::string& Combo, std::function<void()> Fn );
        void RemoveShortcut( uint32_t Id );
        static bool ParseShortcut( const std::string& Combo, Key& K, uint8_t& Mods );
        static std::string ShortcutText( Key K, uint8_t Mods ); // "Ctrl+Shift+S"

        // ---- drag and drop (see Widget::OnDragStart / AcceptDrop / OnDrop)
        bool IsDragging() const { return DragOn; }
        const DragData* Dragged() const { return DragOn ? &DragNow : nullptr; }
        Widget* DropTarget() const { return DropW; }
        void CancelDrag();
        float DragThreshold = 6;

        // ---- developer tools: F12 = inspector (hover anything), Ctrl+F12 = performance stats
        bool ShowInspector = false, ShowStats = false;
        bool DevToolKeys = true;

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

        // ---- timers (UI thread). Every: repeating; After: once. The app sleeps until the next one is due.
        //   Ctx.Every( 2.0, [&] { PollStats(); } );
        uint32_t Every( double Seconds, std::function<void()> Fn );
        uint32_t After( double Seconds, std::function<void()> Fn );
        void CancelTimer( uint32_t Id );
        // Reload the theme file whenever it changes on disk (edit colors live). See tg/theme.h for the format.
        void WatchTheme( const std::string& Path );

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
        struct Timer { uint32_t Id; double Due, Period; std::function<void()> Fn; };
        std::vector<Timer> Timers;
        uint32_t NextTimer = 1;
        double TimerNow() const;
        void RunTimers();
        Widget* KeyHotW = nullptr;
        bool DragOn = false;
        DragData DragNow;
        Widget *DragSrc = nullptr, *DropW = nullptr;
        Vec2 PressAt;
        void DrawDrag();
        bool FocusRing = false;
        struct ShortcutEntry { uint32_t Id; Key K; uint8_t Mods; std::function<void()> Fn; };
        std::vector<ShortcutEntry> Shortcuts;
        uint32_t NextShortcut = 1;
        bool RunShortcuts( Key K, uint8_t Mods );
        void PopupKeys( Key K, bool& Handled );
        // stats
        double FrameCpu = 0, LayoutCpu = 0, FpsWindowStart = 0;
        int FpsCount = 0;
        float Fps = 0;
        size_t LastVerts = 0, LastCmds = 0, LastIdx = 0;
        void DrawDevTools();
        std::vector<std::pair<const void*, std::function<void()>>> FrameHooks, Pumps;

        struct PostQueue;
        std::unique_ptr<PostQueue> Posts;

        void UpdateHover();
        Widget* HitTestAll( Vec2 P );
        bool Dispatch( Widget* Target, const Event& E ); // bubbles up
    };
}
