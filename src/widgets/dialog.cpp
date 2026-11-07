#include "tg/widgets/dialog.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/widgets/button.h"
#include "tg/widgets/label.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    Dialog::Dialog( std::string Title )
    {
        Interactive = true;
        Focusable = true;
        Layout.Width = Grow();  // full-screen layer on the overlay
        Layout.Height = Grow();
        Card = &Add<Container>( Dir::Column, 12, Insets( 20, 18 ) );
        auto& T = Card->Add<Label>( std::move( Title ) );
        T.Style.TextFont = GetTheme().Title;
        BodyC = &Card->Add<Container>( Dir::Column, 10 );
        Buttons = &Card->Add<Container>( Dir::Row, 8 );
        Buttons->Layout.MainAlign = Align::End;
        Buttons->Layout.Padding = Insets( 0, 6, 0, 0 );
    }

    tg::Button& Dialog::AddButton( std::string Text, std::function<void()> Fn )
    {
        int Index = ButtonCount++;
        auto& B = Buttons->Add<tg::Button>( std::move( Text ) );
        B.OnClick = [ this, Index, Fn ] { if ( Fn ) Fn(); Close( Index ); };
        return B;
    }

    void Dialog::Close( int Result )
    {
        if ( Closed ) return;
        Closed = true;
        Visible = false;
        auto Cb = std::move( OnResult );
        if ( Cb ) Cb( Result );
        if ( Context* C = GetContext() )
        {
            if ( C->Focused() == this ) C->SetFocus( nullptr );
            C->Post( [ C, this ] { C->Overlay().Remove( this ); } );
            C->InvalidateLayout();
        }
    }

    void Dialog::Arrange( Rect R )
    {
        Bounds = R;
        float W = std::min( Style.Width, R.w - 32 );
        float H = std::min( Card->MeasureCached( W ).y, R.h - 32 );
        Card->Arrange( { std::floor( R.x + ( R.w - W ) * 0.5f ), std::floor( R.y + ( R.h - H ) * 0.42f ), W, H } );
    }

    void Dialog::Draw( DrawList& D )
    {
        Animate( FadeT, 1.f, 14.f );
        size_t V0 = D.Vtx.size();
        D.RectFilled( Bounds, Style.Backdrop.Resolve() );
        Rect C = Card->Bounds;
        D.Shadow( C, Style.Radius, Color( 0, 0, 0, 140 ), 18, { 0, 8 } );
        D.RectFilled( C, Style.Fill.Resolve(), Style.Radius );
        D.RectStroke( C, Style.Stroke.Resolve(), Style.Radius );
        DrawChildren( D );
        D.FadeFrom( V0, FadeT );
    }

    Widget* Dialog::HitTest( Vec2 P )
    {
        if ( !Visible ) return nullptr;
        if ( Widget* H = Card->HitTest( P ) ) return H;
        return Bounds.Contains( P ) ? this : nullptr; // the backdrop swallows clicks: modal
    }

    bool Dialog::OnEvent( const Event& E )
    {
        if ( E.Type == EventType::KeyDown )
        {
            if ( E.KeyCode == Key::Escape ) { Close( -1 ); return true; }
            if ( ( E.KeyCode == Key::Enter || E.KeyCode == Key::KpEnter ) && ButtonCount > 0 )
            {
                auto& Last = Buttons->Children().back();
                if ( auto* B = dynamic_cast<tg::Button*>( Last.get() ) ) { auto Fn = B->OnClick; if ( Fn ) Fn(); }
                return true;
            }
            return false;
        }
        // eat every mouse event that reaches the backdrop
        return E.Type == EventType::MouseDown || E.Type == EventType::MouseUp || E.Type == EventType::MouseWheel;
    }

    Dialog& OpenDialog( Context& Ctx, std::string Title )
    {
        Ctx.ClosePopups();
        auto& D = Ctx.Overlay().Add<Dialog>( std::move( Title ) );
        Ctx.SetFocus( &D ); // Escape / Enter come to us
        return D;
    }

    Dialog& Confirm( Context& Ctx, std::string Title, std::string Message, std::vector<std::string> Btns, std::function<void( int )> OnResult )
    {
        Dialog& D = OpenDialog( Ctx, std::move( Title ) );
        auto& L = D.Body().Add<Label>( std::move( Message ), true );
        L.Style.TextColor = Tok::TextDim;
        for ( size_t i = 0; i < Btns.size(); i++ )
        {
            auto& B = D.AddButton( Btns[ i ] );
            if ( i + 1 == Btns.size() ) B.Style = Button::Primary();
        }
        D.OnResult = std::move( OnResult );
        return D;
    }
}
