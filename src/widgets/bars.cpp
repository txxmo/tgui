#include "tg/widgets/bars.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/widgets/label.h"
#include <cmath>

namespace tg
{
    ToolButton::ToolButton( std::string Icon, std::string Tip, std::function<void()> Fn, bool* C ) : Button( std::move( Icon ), std::move( Fn ) ), Checked( C )
    {
        Style = Button::Ghost();
        Style.Padding = { 8, 6 };
        Tooltip = std::move( Tip );
        Focusable = true;
    }

    void ToolButton::BindSet( const BindValue& V )
    {
        if ( !Checked ) { Button::BindSet( V ); return; }
        bool On = !V.V.empty() && V.V[ 0 ] >= 0.5;
        if ( On != *Checked && OnClick ) OnClick(); // the toggle's own handler flips it
        else *Checked = On;
        Redraw();
    }

    Vec2 ToolButton::Measure( float A )
    {
        Vec2 S = Button::Measure( A );
        return { std::max( S.x, S.y ), S.y }; // icon-only buttons are square
    }

    void ToolButton::Draw( DrawList& D )
    {
        if ( IsOn() ) D.RectFilled( Bounds, GetTheme()[ Tok::Accent ].Fade( 0.22f ), Style.Radius );
        Button::Draw( D );
        if ( IsOn() ) D.RectFilled( { Bounds.x + 6, Bounds.Bottom() - 2, Bounds.w - 12, 2 }, GetTheme()[ Tok::Accent ], 1 );
    }

    // ------------------------------------------------------------ Toolbar

    Toolbar::Toolbar() : Container( Dir::Row, 2, Insets( 6, 4 ) ) { Layout.CrossAlign = Align::Center; }

    ToolButton& Toolbar::Tool( std::string Icon, std::string Tip, std::function<void()> Fn )
    {
        return Add<ToolButton>( std::move( Icon ), std::move( Tip ), std::move( Fn ) );
    }

    ToolButton& Toolbar::ToolToggle( std::string Icon, std::string Tip, bool* V, std::function<void( bool )> Changed )
    {
        auto& B = Add<ToolButton>( std::move( Icon ), std::move( Tip ), std::function<void()>{}, V );
        ToolButton* P = &B;
        B.OnClick = [ P, Changed ] {
            if ( !P->Checked ) return;
            *P->Checked = !*P->Checked;
            if ( Changed ) Changed( *P->Checked );
        };
        return B;
    }

    void Toolbar::Separator()
    {
        auto& S = Add<Box>();
        S.Style.Fill = Tok::Border;
        S.Layout.Width = Px( 1 );
        S.Layout.Height = Px( 20 );
    }

    void Toolbar::Spacer() { Add<Widget>().Layout.Width = Grow(); }

    void Toolbar::Draw( DrawList& D )
    {
        D.RectFilled( Bounds, GetTheme()[ Tok::Surface ] );
        D.RectFilled( { Bounds.x, Bounds.Bottom() - 1, Bounds.w, 1 }, GetTheme()[ Tok::Border ] );
        DrawChildren( D );
    }

    // ------------------------------------------------------------ StatusBar

    StatusBar::StatusBar() : Container( Dir::Row, 14, Insets( 12, 3 ) ) { Layout.CrossAlign = Align::Center; }

    Label& StatusBar::Item( std::string Text )
    {
        auto& L = Add<Label>( std::move( Text ) );
        L.Style.TextFont = GetTheme().Small;
        L.Style.TextColor = Tok::TextDim;
        return L;
    }

    void StatusBar::Separator()
    {
        auto& S = Add<Box>();
        S.Style.Fill = Tok::Border;
        S.Layout.Width = Px( 1 );
        S.Layout.Height = Px( 12 );
    }

    void StatusBar::Spacer() { Add<Widget>().Layout.Width = Grow(); }

    void StatusBar::Draw( DrawList& D )
    {
        D.RectFilled( Bounds, GetTheme()[ Tok::Surface ] );
        D.RectFilled( { Bounds.x, Bounds.y, Bounds.w, 1 }, GetTheme()[ Tok::Border ] );
        DrawChildren( D );
    }
}
