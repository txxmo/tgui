// Container — a Box with one-line helpers, so building a panel reads like ImGui:
//
//   auto& W = App.Window( "Settings" );
//   W.Checkbox( "Enabled", &Enabled );
//   W.Slider( "Volume", &Volume, 0, 100 );
//   W.Combo( "Quality", { "Low", "Medium", "High" }, &Quality );
//   W.Button( "Apply", [] { Apply(); } );
//
// Every helper returns the real widget, so you can keep styling it:  W.Checkbox( "x" ).Style.Radius = 0;
// Labeled controls (Combo, ColorPicker, ...) are a row: [label | control].
// Sliders stack like tgui did: [label ..... value] with the bar underneath.
#pragma once
#include "box.h"
#include "listbox.h"
#include <functional>
#include <string>
#include <vector>

namespace tg
{
    class Label;
    class Button;
    class Checkbox;
    class Toggle;
    class Slider;
    class TextInput;
    class Combo;
    class MultiCombo;
    class ColorPicker;
    class ColorPanel;
    class ProgressBar;
    class Table;
    class Groupbox;
    class Keybind;
    class TabView;
    struct TableColumn;

    class Container : public Box
    {
    public:
        explicit Container( Dir D = Dir::Column, float Gap = 10, Insets Padding = {} ) : Box( D, Gap, Padding ) {}

        float LabelWidth = 0; // fixed width for the label column of labeled rows (0 = fit the text)

        // text
        tg::Label& Text( std::string Text, bool Wrap = false );
        tg::Label& Heading( std::string Text );
        tg::Label& Caption( std::string Text ); // small, dim

        // controls
        tg::Button& Button( std::string Text, std::function<void()> OnClick = {} );
        tg::Checkbox& Checkbox( std::string Text, bool* Value = nullptr );
        tg::Toggle& Toggle( std::string Text, bool* Value = nullptr );
        tg::Slider& Slider( std::string Label, float* Value, float Min, float Max, float Step = 0, const char* Format = "%.2f" );
        tg::Slider& SliderInt( std::string Label, int* Value, int Min, int Max );
        tg::TextInput& Input( std::string Placeholder, std::string* Value = nullptr );
        tg::Combo& Combo( std::string Label, std::vector<std::string> Items, int* Index = nullptr );
        tg::MultiCombo& MultiCombo( std::string Label, std::vector<std::string> Items, std::vector<bool>* Selected = nullptr );
        tg::ListBox& List( std::vector<std::string> Items, SelectMode Mode = SelectMode::Single, int* Index = nullptr );
        tg::ColorPicker& ColorPicker( std::string Label, tg::Color* Value = nullptr, bool Alpha = true );
        tg::ColorPanel& ColorPanel( tg::Color* Value = nullptr, bool Alpha = true );
        tg::ProgressBar& Progress( float Value = 0 );
        tg::Keybind& Keybind( std::string Label, tg::Key* Value = nullptr ); // [label ....... [ KEY ]]
        // W.Table( { "Name", "CPU", "Memory" } ) — Grow columns; tweak with T.Column( i ).Width = Px( 80 )
        tg::Table& Table( std::vector<std::string> Columns, SelectMode Mode = SelectMode::Single, int* Selected = nullptr );

        // structure (all return Containers, so the helpers chain)
        // tgui-style organisation
        Container& Tab( std::string Name );            // adds a tab (creates the TabView on first use); returns its page
        tg::Groupbox& Group( std::string Title );      // titled, scrolling frame; fills the space it's given
        Container& Columns( float Gap = 8 );           // a full-height row: put Groups side by side in it
        tg::TabView* Tabs();                           // the TabView made by Tab(), if any

        Container& Row( float Gap = 8 );
        Container& Column( float Gap = 8 );
        Container& Card( std::string Title = {} );
        Container& Labeled( std::string Label ); // [label | ...] row; add the control to the returned row
        void Separator();
        void Spacer();
    };
}
