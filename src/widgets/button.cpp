#include "tg/widgets/button.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    ButtonStyle& Button::Defaults()
    {
        static ButtonStyle S;
        return S;
    }

    ButtonStyle Button::Primary()
    {
        ButtonStyle S = Defaults();
        S.Fill = Tok::Accent;
        S.Stroke = Color( 0, 0, 0, 0 );
        S.TextColor = Tok::TextOnAccent;
        return S;
    }

    ButtonStyle Button::Danger()
    {
        ButtonStyle S = Defaults();
        S.Fill = Tok::Danger;
        S.Stroke = Color( 0, 0, 0, 0 );
        S.TextColor = Color( 255, 255, 255 );
        return S;
    }

    ButtonStyle Button::Ghost()
    {
        ButtonStyle S = Defaults();
        S.Fill = Color( 0, 0, 0, 0 );
        S.Stroke = Color( 0, 0, 0, 0 );
        S.HoverOverlay = { 255, 255, 255, 14 };
        return S;
    }

    Button::Button( std::string T, std::function<void()> Fn ) : Text( std::move( T ) ), OnClick( std::move( Fn ) ), Style( Defaults() )
    {
        Interactive = true;
    }

    Vec2 Button::Measure( float )
    {
        const Font& F = *GetTheme().Bold;
        return { std::ceil( F.Width( Text ) ) + Style.Padding.Horizontal(), F.LineHeight + Style.Padding.Vertical() };
    }

    void Button::Draw( DrawList& D )
    {
        const ButtonStyle& S = Style;
        const Font& F = *GetTheme().Bold;
        Animate( HoverT, Enabled && IsHovered() ? 1.f : 0.f );
        Animate( PressT, IsPressed() && IsHovered() ? 1.f : 0.f, 30.f );

        float Alpha = Enabled ? 1.f : 0.45f;
        D.RectFilled( Bounds, S.Fill.Resolve().Fade( Alpha ), S.Radius );
        D.RectFilled( Bounds, S.HoverOverlay.Fade( HoverT ), S.Radius );
        D.RectFilled( Bounds, S.PressOverlay.Fade( PressT ), S.Radius );
        if ( S.Ripple && RippleT < 1 )
        {
            if ( Context* C = GetContext() )
            {
                RippleT = std::min( 1.f, RippleT + C->DeltaTime() / 0.55f );
                C->RequestFrame();
            }
            else RippleT = 1;
            // radius reaches the farthest corner; fades as it spreads, clipped to the rounded button
            float Far = 0;
            for ( Vec2 K : { Bounds.Pos(), Vec2{ Bounds.Right(), Bounds.y }, Bounds.Max(), Vec2{ Bounds.x, Bounds.Bottom() } } )
            {
                Vec2 Dv = K - RippleAt;
                Far = std::max( Far, std::sqrt( Dv.x * Dv.x + Dv.y * Dv.y ) );
            }
            float R = Far * EaseOutCubic( RippleT );
            Vec2 Pts[ 40 ];
            for ( int i = 0; i < 40; i++ )
            {
                float A = 6.2831853f * i / 40;
                Pts[ i ] = { RippleAt.x + std::cos( A ) * R, RippleAt.y + std::sin( A ) * R };
            }
            Color Rc = S.TextColor.Resolve();
            Rc.a = uint8_t( 70 * ( 1 - RippleT ) * Alpha );
            D.ConvexFillInRounded( Pts, 40, Rc, Bounds, S.Radius );
        }
        D.RectStroke( Bounds, S.Stroke.Resolve().Fade( Alpha ), S.Radius );

        float W = F.Width( Text );
        Vec2 P = { std::floor( Bounds.x + ( Bounds.w - W ) * 0.5f ), std::floor( Bounds.y + ( Bounds.h - F.LineHeight ) * 0.5f ) };
        D.Text( F, P, Text, S.TextColor.Resolve().Fade( Alpha ) );
    }

    bool Button::OnEvent( const Event& E )
    {
        if ( E.Button != MouseButton::Left ) return false;
        if ( E.Type == EventType::MouseDown )
        {
            RippleAt = E.Pos;
            RippleT = 0;
            Redraw();
            return true;
        }
        if ( E.Type == EventType::MouseUp )
        {
            if ( IsHovered() && OnClick ) { auto Fn = OnClick; Fn(); } // copy: the handler may delete us
            return true;
        }
        return false;
    }
}
