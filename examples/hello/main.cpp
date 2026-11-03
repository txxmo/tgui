// The old tgui README example, on tg: a window with tabs and groupboxes.
// No asset files needed (the font is built in).
#include "tg/app.h"
#include <cstdio>

int main()
{
    tg::App App( "hello tg", 900, 600 );

    static bool BooleanEx = true;
    static float SliderEx = 40, fSliderEx = 12;
    static int iComboEx = 0;
    static tg::Color ColorEx = tg::Color::Hex( 0x6EB7D4 );

    /* Create our example window */
    auto& ExampleWindow = App.Window( "Test Window", { 40, 40 }, { 440, 320 } );
    ExampleWindow.Chrome = tg::Window::Classic();
    ExampleWindow.Footer = "v.2.5";
    ExampleWindow.Layout.Padding = tg::Insets( 8 );

    /* Create some tabs */
    auto& Tab1 = ExampleWindow.Tab( "Tab 1" );
    auto& Tab2 = ExampleWindow.Tab( "Tab 2" );
    ExampleWindow.Tab( "Tab 3" ).Caption( "Nothing here yet." );

    /* Groupboxes side by side; they scroll if their contents don't fit */
    auto& Groups = Tab1.Columns();
    auto& ExampleGroup = Groups.Group( "Example Group" );
    {
        ExampleGroup.Checkbox( "Checkbox", &BooleanEx );
        ExampleGroup.Slider( "Slider float", &SliderEx, 0, 100 );
        ExampleGroup.Slider( "Slider int", &fSliderEx, 0, 100, 1 );
        ExampleGroup.Combo( "", { "Item 1", "Item 2", "Item 3" }, &iComboEx );
    }
    auto& ExampleGroup2 = Groups.Group( "Example Group 2" );
    {
        ExampleGroup2.ColorPicker( "Color", &ColorEx );
        ExampleGroup2.Button( "Print", [] { std::printf( "checkbox=%d slider=%.2f combo=%d\n", BooleanEx, SliderEx, iComboEx ); } );
    }

    Tab2.Columns().Group( "Another Group" ).Checkbox( "Another checkbox" );

    return App.Run();
}
