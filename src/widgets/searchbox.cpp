#include "tg/widgets/searchbox.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/widgets/textinput.h"
#include <cmath>

namespace tg
{
    SearchBox::SearchBox( std::string Ph, std::string* B ) : Bind( B )
    {
        Interactive = true;
        Text = &Add<TextInput>( std::move( Ph ) );
        Text->Style.Fill = Color( 0, 0, 0, 0 );
        Text->Style.Stroke = Color( 0, 0, 0, 0 );
        Text->Style.StrokeFocused = Color( 0, 0, 0, 0 );
        Text->Style.Padding = Insets( 4, 8 );
        if ( B ) Text->SetText( *B );
        Text->OnChange = [ this ]( const std::string& T ) { if ( Bind ) *Bind = T; if ( OnChange ) OnChange( T ); Redraw(); };
        Text->OnSubmit = [ this ]( const std::string& T ) { if ( OnSubmit ) OnSubmit( T ); };
    }

    const std::string& SearchBox::GetText() const { return Text->GetText(); }
    void SearchBox::SetText( std::string T ) { Text->SetText( std::move( T ) ); }

    Vec2 SearchBox::Measure( float Avail ) { return { std::min( Avail, 240.f ), Text->MeasureCached( 100 ).y }; }

    void SearchBox::Arrange( Rect R )
    {
        Bounds = R;
        ClearR = { R.Right() - 28, R.y, 24, R.h };
        Text->Arrange( { R.x + 28, R.y, R.w - 28 - 28, R.h } );
    }

    void SearchBox::Draw( DrawList& D )
    {
        float Rad = std::min( Radius, Bounds.h * 0.5f );
        D.RectFilled( Bounds, Fill.Resolve(), Rad );
        D.RectStroke( Bounds, ( Text->IsFocused() ? StrokeFocused : Stroke ).Resolve(), Rad );
        // magnifier
        Color Ic = Icon.Resolve();
        Vec2 C = { Bounds.x + 16, Bounds.y + Bounds.h * 0.5f - 1 };
        D.CircleStroke( C, 5.5f, Ic, 1.5f );
        D.Line( C + Vec2( 4, 4 ), C + Vec2( 8, 8 ), Ic, 1.6f );
        Text->Draw( D );
        if ( !Text->GetText().empty() )
        {
            Vec2 X = ClearR.Center();
            if ( HoverClear ) D.CircleFilled( X, 8, GetTheme()[ Tok::Raised ] );
            D.Line( X + Vec2( -3.5f, -3.5f ), X + Vec2( 3.5f, 3.5f ), Ic, 1.5f );
            D.Line( X + Vec2( 3.5f, -3.5f ), X + Vec2( -3.5f, 3.5f ), Ic, 1.5f );
        }
    }

    Widget* SearchBox::HitTest( Vec2 P )
    {
        if ( !Visible || !Bounds.Contains( P ) ) return nullptr;
        if ( ClearR.Contains( P ) && !Text->GetText().empty() ) return this;
        return Text;
    }

    bool SearchBox::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseMove: { bool H = ClearR.Contains( E.Pos ); if ( H != HoverClear ) { HoverClear = H; Redraw(); } return false; }
        case EventType::MouseLeave: HoverClear = false; Redraw(); return false;
        case EventType::MouseDown: return E.Button == MouseButton::Left;
        case EventType::MouseUp:
            if ( E.Button == MouseButton::Left && ClearR.Contains( E.Pos ) ) { SetText( {} ); if ( GetContext() ) GetContext()->SetFocus( Text ); }
            return true;
        case EventType::KeyDown: // bubbled from the field
            if ( E.KeyCode == Key::Escape && !Text->GetText().empty() ) { SetText( {} ); return true; }
            return false;
        default:
            return false;
        }
    }
}
