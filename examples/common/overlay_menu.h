// A tgui-style menu shared by the "bring your own window/renderer" examples (Win32 D3D11 / D3D9):
// classic window, tabs, groupboxes, a menu keybind. Toggle with Insert, like the old tgui.
#pragma once
#include "tg/tg.h"

struct OverlayState
{
    bool Esp = true, Boxes = true, Names = false, Crosshair = false, Streamer = false;
    float Opacity = 0.85f;
    int FpsCap = 144, Position = 1;
    tg::Color Tint = tg::Color::Hex( 0x6EB7D4 );
    tg::Key AimKey = tg::Key::Mouse5, MenuKey = tg::Key::Insert;
    tg::Window* Menu = nullptr;
};

inline tg::Window& BuildOverlayMenu( tg::Context& Ctx, OverlayState& S )
{
    using namespace tg;
    Widget& Root = Ctx.Root();
    Root.Layout.Direction = Dir::Free; // the menu floats over whatever the host draws

    auto& W = Root.Add<Window>( "tg overlay", Vec2{ 60, 60 }, Vec2{ 480, 360 } );
    W.Chrome = Window::Classic();
    W.Footer = "Insert = show/hide";
    W.Layout.Padding = Insets( 8 );
    W.Closable = false;
    S.Menu = &W;

    auto& Cols = W.Tab( "Visuals" ).Columns();
    auto& G1 = Cols.Group( "ESP" );
    G1.LabelWidth = 0;
    G1.Checkbox( "Enabled", &S.Esp );
    G1.Checkbox( "Boxes", &S.Boxes );
    G1.Checkbox( "Names", &S.Names );
    G1.Checkbox( "Crosshair", &S.Crosshair );
    G1.Slider( "Opacity", &S.Opacity, 0, 1 );
    G1.Combo( "", { "Top left", "Top right", "Bottom left", "Bottom right" }, &S.Position );
    auto& G2 = Cols.Group( "Colors" );
    G2.ColorPicker( "Tint", &S.Tint );
    G2.Keybind( "Aim key", &S.AimKey ).ShowMode = true;
    G2.Toggle( "Streamer mode", &S.Streamer );

    auto& Misc = W.Tab( "Misc" ).Columns();
    auto& G3 = Misc.Group( "Performance" );
    G3.SliderInt( "FPS cap", &S.FpsCap, 30, 240 );
    G3.Keybind( "Menu key", &S.MenuKey ).AllowModes = false;
    auto& G4 = Misc.Group( "Info" );
    G4.Caption( "Rendered by tg into the host's own swap chain. Input comes from the host's WndProc via tg::win32::HandleMessage." );

    // Insert (or whatever the menu key is bound to) shows/hides the menu.
    Ctx.OnKeyEvent = [ &S ]( Key K, bool Down ) {
        if ( !Down || K != S.MenuKey || K == Key::None ) return false;
        if ( S.Menu->IsOpen() ) S.Menu->Close();
        else S.Menu->Show();
        return true;
    };
    return W;
}
