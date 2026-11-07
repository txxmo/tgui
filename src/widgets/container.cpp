#include "tg/widgets/container.h"
#include "tg/font.h"
#include "tg/widgets/button.h"
#include "tg/widgets/checkbox.h"
#include "tg/widgets/colorpicker.h"
#include "tg/widgets/combo.h"
#include "tg/widgets/label.h"
#include "tg/widgets/progress.h"
#include "tg/widgets/charts.h"
#include "tg/widgets/choice.h"
#include "tg/widgets/datepicker.h"
#include "tg/widgets/menubar.h"
#include "tg/widgets/searchbox.h"
#include "tg/widgets/taginput.h"
#include "tg/widgets/textarea.h"
#include "tg/widgets/collapsible.h"
#include "tg/widgets/extras.h"
#include "tg/widgets/groupbox.h"
#include "tg/widgets/number.h"
#include "tg/widgets/rangeslider.h"
#include "tg/widgets/splitter.h"
#include "tg/widgets/treeview.h"
#include "tg/widgets/keybind.h"
#include "tg/widgets/slider.h"
#include "tg/widgets/tabview.h"
#include "tg/widgets/table.h"
#include "tg/widgets/textinput.h"
#include "tg/widgets/toggle.h"
#include <cstdio>
#include <memory>

namespace tg
{
    tg::Label& Container::Text( std::string T, bool Wrap ) { return Add<tg::Label>( std::move( T ), Wrap ); }

    tg::Label& Container::Heading( std::string T )
    {
        auto& L = Add<tg::Label>( std::move( T ) );
        L.Style.TextFont = GetTheme().Bold;
        return L;
    }

    tg::Label& Container::Caption( std::string T )
    {
        auto& L = Add<tg::Label>( std::move( T ), true );
        L.Style.TextFont = GetTheme().Small;
        L.Style.TextColor = Tok::TextDim;
        return L;
    }

    tg::Button& Container::Button( std::string T, std::function<void()> Fn ) { return Add<tg::Button>( std::move( T ), std::move( Fn ) ); }
    tg::Checkbox& Container::Checkbox( std::string T, bool* V ) { return Add<tg::Checkbox>( std::move( T ), V ); }
    tg::Toggle& Container::Toggle( std::string T, bool* V ) { return Add<tg::Toggle>( std::move( T ), V ); }

    Container& Container::Labeled( std::string Label )
    {
        auto& R = Row( 10 );
        R.Layout.CrossAlign = Align::Center;
        if ( !Label.empty() )
        {
            auto& L = R.Add<tg::Label>( std::move( Label ) );
            L.Style.TextColor = Tok::TextDim;
            if ( LabelWidth > 0 ) L.Layout.Width = Px( LabelWidth );
        }
        return R;
    }

    tg::Slider& Container::Slider( std::string Label, float* V, float Min, float Max, float Step, const char* Format )
    {
        // tgui layout: [label ........ value] on one line, the slider full width underneath.
        auto& Col = Column( 2 );
        auto& Top = Col.Row( 8 );
        auto& L = Top.Add<tg::Label>( std::move( Label ) );
        L.Style.TextColor = Tok::TextDim;
        L.Layout.Width = Grow();
        auto& Val = Top.Add<tg::Label>();
        Val.Style.HAlign = Align::End;
        auto& S = Col.Add<tg::Slider>( V, Min, Max, Step );
        std::string Fmt = Step >= 1 ? "%.0f" : Format;
        auto Show = [ &Val, Fmt ]( float X ) { char B[ 32 ]; std::snprintf( B, sizeof( B ), Fmt.c_str(), X ); Val.SetText( B ); };
        Show( S.GetValue() );
        S.OnChange = Show;
        return S;
    }

    tg::Slider& Container::SliderInt( std::string Label, int* V, int Min, int Max )
    {
        // Slider works in floats: keep a float mirror and write the int back on change.
        auto Mirror = std::make_shared<float>( V ? float( *V ) : float( Min ) );
        auto& S = Slider( std::move( Label ), Mirror.get(), float( Min ), float( Max ), 1 );
        auto Show = S.OnChange;
        S.OnChange = [ V, Mirror, Show ]( float X ) { if ( V ) *V = int( X ); if ( Show ) Show( X ); };
        return S;
    }

    tg::TextInput& Container::Input( std::string Placeholder, std::string* V )
    {
        auto& T = Add<tg::TextInput>( std::move( Placeholder ) );
        if ( V )
        {
            T.SetText( *V );
            T.OnChange = [ V ]( const std::string& S ) { *V = S; };
        }
        return T;
    }

    tg::Combo& Container::Combo( std::string Label, std::vector<std::string> Items, int* Index )
    {
        auto& C = Labeled( std::move( Label ) ).Add<tg::Combo>( std::move( Items ), Index );
        C.Layout.Width = Grow();
        return C;
    }

    tg::MultiCombo& Container::MultiCombo( std::string Label, std::vector<std::string> Items, std::vector<bool>* Sel )
    {
        auto& C = Labeled( std::move( Label ) ).Add<tg::MultiCombo>( std::move( Items ), Sel );
        C.Layout.Width = Grow();
        return C;
    }

    tg::ListBox& Container::List( std::vector<std::string> Items, SelectMode Mode, int* Index )
    {
        return Add<tg::ListBox>( std::move( Items ), Mode, Index );
    }

    tg::ColorPicker& Container::ColorPicker( std::string Label, tg::Color* V, bool Alpha )
    {
        auto& R = Labeled( std::move( Label ) );
        if ( !R.Children().empty() ) R.Children()[ 0 ]->Layout.Width = Grow();
        return R.Add<tg::ColorPicker>( V, Alpha );
    }

    tg::ColorPanel& Container::ColorPanel( tg::Color* V, bool Alpha ) { return Add<tg::ColorPanel>( V, Alpha ); }
    tg::ProgressBar& Container::Progress( float V ) { return Add<tg::ProgressBar>( V ); }
    tg::Keybind& Container::Keybind( std::string Label, tg::Key* V ) { return Add<tg::Keybind>( std::move( Label ), V ); }

    tg::RadioGroup& Container::Radio( std::vector<std::string> O, int* I, Dir D ) { return Add<tg::RadioGroup>( std::move( O ), I, D ); }
    tg::Segmented& Container::Segmented( std::vector<std::string> O, int* I ) { return Add<tg::Segmented>( std::move( O ), I ); }

    tg::NumberInput& Container::Number( std::string Label, float* V, float Min, float Max, float Step, const char* Fmt )
    {
        auto& R = Labeled( std::move( Label ) );
        auto& N = R.Add<tg::NumberInput>( V, Min, Max, Step, Fmt );
        N.Layout.Width = Grow();
        return N;
    }

    tg::NumberInput& Container::NumberInt( std::string Label, int* V, int Min, int Max, int Step )
    {
        auto Mirror = std::make_shared<float>( V ? float( *V ) : float( Min ) );
        auto& N = Number( std::move( Label ), Mirror.get(), float( Min ), float( Max ), float( Step ), "%.0f" );
        N.OnChange = [ V, Mirror ]( float X ) { if ( V ) *V = int( X ); };
        return N;
    }

    tg::RangeSlider& Container::Range( std::string Label, float* Lo, float* Hi, float Min, float Max, float Step, const char* Fmt )
    {
        // tgui layout like Slider: [label ...... lo - hi] with the bar underneath
        auto& Col = Column( 2 );
        auto& Top = Col.Row( 8 );
        auto& L = Top.Add<tg::Label>( std::move( Label ) );
        L.Style.TextColor = Tok::TextDim;
        L.Layout.Width = Grow();
        auto& Val = Top.Add<tg::Label>();
        auto& S = Col.Add<tg::RangeSlider>( Lo, Hi, Min, Max, Step );
        std::string F = Fmt ? Fmt : "%.0f";
        auto Show = [ &Val, F ]( float A, float B ) {
            char Ba[ 32 ], Bb[ 32 ];
            std::snprintf( Ba, sizeof( Ba ), F.c_str(), A );
            std::snprintf( Bb, sizeof( Bb ), F.c_str(), B );
            Val.SetText( std::string( Ba ) + " \xE2\x80\x93 " + Bb );
        };
        Show( S.GetLow(), S.GetHigh() );
        S.OnChange = Show;
        return S;
    }

    tg::TreeView& Container::Tree() { return Add<tg::TreeView>(); }
    tg::Collapsible& Container::Collapsible( std::string Title, bool* Open )
    {
        auto& C = Add<tg::Collapsible>( std::move( Title ), Open );
        C.LabelWidth = LabelWidth;
        return C;
    }
    tg::Splitter& Container::Split( Dir D, float Ratio ) { return Add<tg::Splitter>( D, Ratio ); }
    tg::Spinner& Container::Spinner( float Size ) { return Add<tg::Spinner>( Size ); }
    tg::Badge& Container::Badge( std::string Text, Paint Fill ) { return Add<tg::Badge>( std::move( Text ), Fill ); }
    tg::Link& Container::Link( std::string Text, std::function<void()> Fn ) { return Add<tg::Link>( std::move( Text ), std::move( Fn ) ); }

    tg::TextArea& Container::TextArea( std::string Placeholder, std::string* V )
    {
        auto& T = Add<tg::TextArea>( std::move( Placeholder ) );
        if ( V ) { T.SetText( *V ); T.Bind = V; }
        return T;
    }
    tg::MenuBar& Container::MenuBar() { return Add<tg::MenuBar>(); }
    tg::TagInput& Container::Tags( std::vector<std::string>* V, std::string Ph ) { return Add<tg::TagInput>( V, std::move( Ph ) ); }
    tg::SearchBox& Container::Search( std::string Ph, std::string* V ) { return Add<tg::SearchBox>( std::move( Ph ), V ); }
    tg::DatePicker& Container::Date( std::string Label, tg::Date* V )
    {
        auto& R = Labeled( std::move( Label ) );
        if ( !R.Children().empty() ) R.Children()[ 0 ]->Layout.Width = Grow();
        return R.Add<tg::DatePicker>( V );
    }
    tg::BarChart& Container::BarChart() { return Add<tg::BarChart>(); }
    tg::DonutChart& Container::Donut() { return Add<tg::DonutChart>(); }
    tg::Gauge& Container::Gauge( std::string Label, float V ) { return Add<tg::Gauge>( std::move( Label ), V ); }
    tg::Sparkline& Container::Sparkline( size_t Cap ) { return Add<tg::Sparkline>( Cap ); }

    tg::Table& Container::Table( std::vector<std::string> Columns, SelectMode Mode, int* Sel )
    {
        auto& T = Add<tg::Table>();
        for ( auto& C : Columns ) T.AddColumn( std::move( C ) );
        T.Mode = Mode;
        T.Bind = Sel;
        return T;
    }

    tg::TabView* Container::Tabs()
    {
        for ( auto& K : Children() )
            if ( auto* T = dynamic_cast<tg::TabView*>( K.get() ) ) return T;
        return nullptr;
    }

    Container& Container::Tab( std::string Name )
    {
        tg::TabView* T = Tabs();
        if ( !T ) T = &Add<tg::TabView>();
        auto& Page = T->AddTab( std::move( Name ) );
        Page.LabelWidth = LabelWidth;
        return Page;
    }

    tg::Groupbox& Container::Group( std::string Title )
    {
        auto& G = Add<tg::Groupbox>( std::move( Title ) );
        G.Layout.Width = Layout.Direction == Dir::Row ? Grow() : Fit();
        G.Layout.Height = Grow();
        G.LabelWidth = LabelWidth;
        return G;
    }

    Container& Container::Columns( float Gap )
    {
        auto& R = Add<Container>( Dir::Row, Gap );
        R.Layout.Height = Grow();
        R.LabelWidth = LabelWidth;
        return R;
    }

    Container& Container::Row( float Gap ) { return Add<Container>( Dir::Row, Gap ); }
    Container& Container::Column( float Gap ) { return Add<Container>( Dir::Column, Gap ); }

    Container& Container::Card( std::string Title )
    {
        auto& C = Add<Container>( Dir::Column, 10, Insets( 14 ) );
        C.Style = Box::Card();
        C.LabelWidth = LabelWidth;
        if ( !Title.empty() ) C.Heading( std::move( Title ) );
        return C;
    }

    void Container::Separator() { AddDivider( *this, Layout.Direction == Dir::Row ); }
    void Container::Spacer() { AddSpacer( *this ); }
}
