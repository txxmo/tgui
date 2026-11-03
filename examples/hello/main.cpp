// The smallest tg program: a window with some controls. No asset files needed (the font is built in).
#include "tg/app.h"
#include <cstdio>

int main()
{
    tg::App App( "hello tg", 900, 600 );

    static bool Enabled = true, VSync = false;
    static float Volume = 0.5f;
    static int Quality = 1;
    static tg::Color Tint = tg::Color::Hex( 0x6EB7D4 );

    auto& W = App.Window( "Settings" );
    W.Checkbox( "Enabled", &Enabled );
    W.Checkbox( "V-Sync", &VSync );
    W.Slider( "Volume", &Volume, 0, 1 );
    W.Combo( "Quality", { "Low", "Medium", "High", "Ultra" }, &Quality );
    W.ColorPicker( "Tint", &Tint );
    W.Button( "Print", [] { std::printf( "enabled=%d vsync=%d volume=%.2f quality=%d\n", Enabled, VSync, Volume, Quality ); } );

    return App.Run();
}
