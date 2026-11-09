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
    class RadioGroup;
    class Segmented;
    class NumberInput;
    class RangeSlider;
    class TreeView;
    class Collapsible;
    class Splitter;
    class Spinner;
    class Badge;
    class Link;
    class TextArea;
    class Terminal;
    class RichText;
    class VirtualList;
    class Toolbar;
    class StatusBar;
    class Knob;
    class XYPad;
    class Pagination;
    class Stepper;
    class Wizard;
    class Accordion;
    class PropertyGrid;
    class TimePicker;
    class DateRangePicker;
    class Swatches;
    class Eyedropper;
    class ImageViewer;
    struct Time;
    class MenuBar;
    class TagInput;
    class SearchBox;
    class DatePicker;
    class BarChart;
    class DonutChart;
    class Gauge;
    class Sparkline;
    struct Date;
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
        tg::RadioGroup& Radio( std::vector<std::string> Options, int* Index = nullptr, Dir Direction = Dir::Column );
        tg::Segmented& Segmented( std::vector<std::string> Options, int* Index = nullptr );
        tg::NumberInput& Number( std::string Label, float* Value, float Min, float Max, float Step = 1, const char* Format = "%g" );
        tg::NumberInput& NumberInt( std::string Label, int* Value, int Min, int Max, int Step = 1 );
        tg::RangeSlider& Range( std::string Label, float* Low, float* High, float Min, float Max, float Step = 0, const char* Format = "%.0f" );
        tg::TreeView& Tree();
        tg::Collapsible& Collapsible( std::string Title, bool* Open = nullptr );
        tg::Splitter& Split( Dir Direction = Dir::Row, float Ratio = 0.5f );
        tg::Spinner& Spinner( float Size = 18 );
        tg::Badge& Badge( std::string Text, Paint Fill = Tok::Accent );
        tg::Link& Link( std::string Text, std::function<void()> OnClick = {} );
        tg::TextArea& TextArea( std::string Placeholder = {}, std::string* Value = nullptr );
        tg::Terminal& Terminal(); // VT terminal: .Spawn() a shell, or .Write() output yourself
        tg::RichText& Rich( std::string Markup );                           // **bold** `code` [link](url) <accent>..</>
        tg::VirtualList& List( size_t Count, std::function<std::string( size_t )> TextOf = {} ); // 100k rows
        tg::Toolbar& Toolbar();
        tg::StatusBar& StatusBar();
        tg::Knob& Knob( std::string Label, float* Value, float Min, float Max, float Step = 0 );
        tg::XYPad& XYPad( std::string Label, Vec2* Value, Vec2 Min = { 0, 0 }, Vec2 Max = { 1, 1 } );
        tg::Pagination& Pagination( int* Page, int PageCount );
        tg::Stepper& Stepper( std::vector<std::string> Steps, int* Current = nullptr );
        tg::Wizard& Wizard();
        tg::Accordion& Accordion();
        tg::PropertyGrid& PropertyGrid();
        tg::TimePicker& Time( std::string Label, tg::Time* Value = nullptr );
        tg::DateRangePicker& DateRange( std::string Label, tg::Date* Start = nullptr, tg::Date* End = nullptr );
        tg::Swatches& Swatches( tg::Color* Value = nullptr, std::vector<tg::Color> Palette = {} );
        tg::Eyedropper& Eyedropper( tg::Color* Value = nullptr );
        tg::ImageViewer& ImageViewer( TextureId Tex, int Width, int Height );
        tg::MenuBar& MenuBar();
        tg::TagInput& Tags( std::vector<std::string>* Value = nullptr, std::string Placeholder = "Add..." );
        tg::SearchBox& Search( std::string Placeholder = "Search", std::string* Value = nullptr );
        tg::DatePicker& Date( std::string Label, tg::Date* Value = nullptr );
        tg::BarChart& BarChart();
        tg::DonutChart& Donut();
        tg::Gauge& Gauge( std::string Label, float Value01 = 0 );
        tg::Sparkline& Sparkline( size_t Capacity = 40 );
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
