// Checkbox. Want a different look? Everything visual is in Draw() below, top to bottom.
#include "tg/widgets/checkbox.h"
#include "tg/widgets/choice.h"
#include "tg/widgets/container.h"
#include "tg/binds.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    CheckboxStyle& Checkbox::Defaults()
    {
        static CheckboxStyle S;
        return S;
    }

    Checkbox::Checkbox( std::string T, bool* B ) : Text( std::move( T ) ), Bind( B ), Style( Defaults() )
    {
        Interactive = true; Focusable = true; KeyActivates = true; // Tab to it, Enter / Space clicks
        CheckT = IsChecked() ? 1.f : 0.f;
    }

    void Checkbox::SetChecked( bool V )
    {
        if ( V == IsChecked() ) return;
        ( Bind ? *Bind : Own ) = V;
        if ( OnChange ) OnChange( V );
        Redraw();
    }

    Vec2 Checkbox::Measure( float )
    {
        const Font& F = *GetTheme().Body;
        return { Style.BoxSize + Style.LabelGap + std::ceil( F.Width( Text ) ), std::max( Style.BoxSize, F.LineHeight ) };
    }

    void Checkbox::Draw( DrawList& D )
    {
        const CheckboxStyle& S = Style;
        const Font& F = *GetTheme().Body;
        Animate( CheckT, IsChecked() ? 1.f : 0.f, IsChecked() ? 11.f : 18.f ); // a bit slower in, so the spin reads
        Animate( HoverT, IsHovered() ? 1.f : 0.f );

        Rect Box = { Bounds.x, std::floor( Bounds.y + ( Bounds.h - S.BoxSize ) * 0.5f ), S.BoxSize, S.BoxSize };

        // 1) empty box + border (border picks up the accent on hover)
        D.RectFilled( Box, S.Box.Resolve(), S.Radius );
        D.RectStroke( Box, Color::Lerp( S.BoxStroke.Resolve(), S.BoxHoverStroke.Resolve(), HoverT * 0.6f ), S.Radius );

        // 2) accent fill fades in when checked
        D.RectFilled( Box, S.Fill.Resolve().Fade( CheckT ), S.Radius );

        // 3) the tick draws itself: first stroke, then the long stroke
        if ( CheckT > 0.01f )
        {
            Vec2 A = { Box.x + Box.w * 0.26f, Box.y + Box.h * 0.53f };
            Vec2 B = { Box.x + Box.w * 0.43f, Box.y + Box.h * 0.69f };
            Vec2 C = { Box.x + Box.w * 0.75f, Box.y + Box.h * 0.33f };
            auto L = []( Vec2 P, Vec2 Q, float t ) { return P + ( Q - P ) * t; };
            Vec2 Pts[ 3 ] = { A, L( A, B, std::min( 1.f, CheckT / 0.4f ) ), L( B, C, std::max( 0.f, ( CheckT - 0.4f ) / 0.6f ) ) };
            if ( S.SpinIn )
            {
                // spin + pop: starts a quarter turn back and small, overshoots a little, settles
                float E = EaseOutBack( CheckT, 2.2f ), Ang = ( 1 - CheckT ) * -1.6f, Sc = 0.35f + 0.65f * E;
                Vec2 Mid = Box.Center();
                float Cs = std::cos( Ang ) * Sc, Sn = std::sin( Ang ) * Sc;
                for ( Vec2& P : Pts )
                {
                    Vec2 Q = P - Mid;
                    P = { Mid.x + Q.x * Cs - Q.y * Sn, Mid.y + Q.x * Sn + Q.y * Cs };
                }
            }
            D.Polyline( Pts, CheckT < 0.4f ? 2 : 3, S.Check.Resolve().Fade( std::min( 1.f, CheckT * 2.5f ) ), S.CheckThickness );
        }

        // 4) label: dim when off, bright when on or hovered (same idea as the old tgui checkbox)
        Color Tc = Color::Lerp( S.TextColorOff.Resolve(), S.TextColor.Resolve(), std::max( CheckT, HoverT ) );
        D.Text( F, { Box.Right() + S.LabelGap, std::floor( Bounds.y + ( Bounds.h - F.LineHeight ) * 0.5f ) }, Text, Tc.Fade( Enabled ? 1 : 0.5f ) );
    }

    bool Checkbox::OnEvent( const Event& E )
    {
        if ( E.Button != MouseButton::Left ) return false;
        if ( E.Type == EventType::MouseDown ) return true;
        if ( E.Type == EventType::MouseUp )
        {
            if ( IsHovered() ) SetChecked( !IsChecked() );
            return true;
        }
        return false;
    }

    // ---- key binds: on / off
    static void OnOffEditor( Container& Into, BindValue& V )
    {
        if ( !BindHas( V ) ) V.V.assign( 1, 1.0 );
        auto& S = Into.Segmented( { "On", "Off" } );
        S.SetIndex( V.V[ 0 ] >= 0.5 ? 0 : 1 );
        S.OnChange = [ &V ]( int i ) { V.V = { i == 0 ? 1.0 : 0.0 }; };
    }

    BindValue Checkbox::BindGet() const { return { { IsChecked() ? 1.0 : 0.0 } }; }
    BindValue Checkbox::BindDefault() const { return { { IsChecked() ? 0.0 : 1.0 } }; } // a bind usually flips it
    void Checkbox::BindSet( const BindValue& V ) { if ( BindHas( V ) ) SetChecked( V.V[ 0 ] >= 0.5 ); }
    void Checkbox::BindEditor( Container& Into, BindValue& V ) { OnOffEditor( Into, V ); }
    std::string Checkbox::BindText( const BindValue& V ) const { return BindHas( V ) && V.V[ 0 ] >= 0.5 ? "on" : "off"; }
}
