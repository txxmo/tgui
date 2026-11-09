#include "tg/widgets/toggle.h"
#include "tg/widgets/choice.h"
#include "tg/widgets/container.h"
#include "tg/binds.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    Toggle::Toggle( std::string T, bool* B ) : Text( std::move( T ) ), Bind( B )
    {
        Interactive = true; Focusable = true; KeyActivates = true; // Tab to it, Enter / Space clicks
        OnT = IsOn() ? 1.f : 0.f;
    }

    void Toggle::SetOn( bool V )
    {
        if ( V == IsOn() ) return;
        ( Bind ? *Bind : Own ) = V;
        if ( OnChange ) OnChange( V );
        Redraw();
    }

    Vec2 Toggle::Measure( float )
    {
        const Font& F = *GetTheme().Body;
        float TextW = Text.empty() ? 0 : Style.LabelGap + std::ceil( F.Width( Text ) );
        return { Style.TrackWidth + TextW, std::max( Style.TrackHeight, F.LineHeight ) };
    }

    void Toggle::Draw( DrawList& D )
    {
        const ToggleStyle& S = Style;
        const Font& F = *GetTheme().Body;
        Animate( OnT, IsOn() ? 1.f : 0.f, 20.f );

        Rect Track = { Bounds.x, std::floor( Bounds.y + ( Bounds.h - S.TrackHeight ) * 0.5f ), S.TrackWidth, S.TrackHeight };
        D.RectFilled( Track, Color::Lerp( S.TrackOff.Resolve(), S.TrackOn.Resolve(), OnT ), S.TrackHeight * 0.5f );

        float R = S.TrackHeight * 0.5f - S.KnobInset;
        float X0 = Track.x + S.KnobInset + R, X1 = Track.Right() - S.KnobInset - R;
        Vec2 Knob = { Lerp( X0, X1, OnT ), Track.y + S.TrackHeight * 0.5f };
        D.CircleFilled( Knob + Vec2( 0, 1 ), R, Color( 0, 0, 0, 50 ) );
        D.CircleFilled( Knob, R, S.Knob.Resolve() );

        if ( !Text.empty() )
            D.Text( F, { Track.Right() + S.LabelGap, std::floor( Bounds.y + ( Bounds.h - F.LineHeight ) * 0.5f ) }, Text, S.TextColor.Resolve() );
    }

    bool Toggle::OnEvent( const Event& E )
    {
        if ( E.Button != MouseButton::Left ) return false;
        if ( E.Type == EventType::MouseDown ) return true;
        if ( E.Type == EventType::MouseUp )
        {
            if ( IsHovered() ) SetOn( !IsOn() );
            return true;
        }
        return false;
    }

    // ---- key binds: on / off (editor shared with Toggle / Checkbox: tg/binds.h)

    BindValue Toggle::BindGet() const { return { { IsOn() ? 1.0 : 0.0 } }; }
    BindValue Toggle::BindDefault() const { return { { IsOn() ? 0.0 : 1.0 } }; } // a bind usually flips it
    void Toggle::BindSet( const BindValue& V ) { if ( BindHas( V ) ) SetOn( V.V[ 0 ] >= 0.5 ); }
    void Toggle::BindEditor( Container& Into, BindValue& V ) { OnOffEditor( Into, V ); }
    std::string Toggle::BindText( const BindValue& V ) const { return BindHas( V ) && V.V[ 0 ] >= 0.5 ? "on" : "off"; }
}
