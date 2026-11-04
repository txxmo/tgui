#include "tg/widgets/container.h"
#include "tg/font.h"
#include "tg/widgets/button.h"
#include "tg/widgets/checkbox.h"
#include "tg/widgets/colorpicker.h"
#include "tg/widgets/combo.h"
#include "tg/widgets/label.h"
#include "tg/widgets/progress.h"
#include "tg/widgets/groupbox.h"
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
