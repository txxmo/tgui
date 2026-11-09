#include "tg/widgets/slider.h"
#include <memory>
#include "tg/widgets/choice.h"
#include "tg/widgets/container.h"
#include "tg/binds.h"
#include "tg/draw.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    Slider::Slider( float* B, float Mn, float Mx, float St ) : Bind( B ), Min( Mn ), Max( Mx ), Step( St )
    {
        Interactive = true; Focusable = true; // arrows adjust
        Own = Mn;
    }

    void Slider::SetValue( float V )
    {
        V = Clamp( V, std::min( Min, Max ), std::max( Min, Max ) );
        if ( Step > 0 ) V = Min + std::round( ( V - Min ) / Step ) * Step;
        if ( V == GetValue() ) return;
        ( Bind ? *Bind : Own ) = V;
        if ( OnChange ) OnChange( V );
        Redraw();
    }

    void Slider::SetFromMouse( float X )
    {
        float L = Bounds.x + Style.KnobRadius, W = Bounds.w - Style.KnobRadius * 2;
        float t = W > 0 ? Clamp( ( X - L ) / W, 0, 1 ) : 0;
        SetValue( Min + ( Max - Min ) * t );
    }

    Vec2 Slider::Measure( float Avail ) { return { std::min( Avail, 160.f ), Style.Height }; }

    void Slider::Draw( DrawList& D )
    {
        const SliderStyle& S = Style;
        Animate( HoverT, IsHovered() || IsPressed() ? 1.f : 0.f );

        float Target = Max != Min ? Clamp( ( GetValue() - Min ) / ( Max - Min ), 0, 1 ) : 0;
        if ( ShownT < 0 ) ShownT = Target;              // first frame: no animation from 0
        float t = Animate( ShownT, Target, IsPressed() ? 30.f : 16.f ); // snappy while dragging, smooth for clicks / binds / code
        float Cy = Bounds.y + Bounds.h * 0.5f;
        float L = Bounds.x + S.KnobRadius, W = Bounds.w - S.KnobRadius * 2;
        Rect Track = { L, Cy - S.TrackHeight * 0.5f, W, S.TrackHeight };

        D.RectFilled( Track, S.Track.Resolve(), S.TrackHeight * 0.5f );
        D.RectFilled( { Track.x, Track.y, W * t, Track.h }, S.TrackFill.Resolve(), S.TrackHeight * 0.5f );

        Vec2 Knob = { L + W * t, Cy };
        float R = S.KnobRadius + HoverT * 1.5f;
        D.CircleFilled( Knob, R + 2, S.KnobStroke.Resolve().Fade( 0.25f * HoverT ) );
        D.CircleFilled( Knob, R, S.KnobStroke.Resolve() );
        D.CircleFilled( Knob, R - 2, S.Knob.Resolve() );
    }

    bool Slider::OnEvent( const Event& E )
    {
        if ( E.Type == EventType::KeyDown )
        {
            float St = Step > 0 ? Step : ( Max - Min ) / 100.f;
            if ( E.Mods & ModShift ) St *= 10;
            switch ( E.KeyCode )
            {
            case Key::Left: case Key::Down: SetValue( GetValue() - St ); return true;
            case Key::Right: case Key::Up: SetValue( GetValue() + St ); return true;
            case Key::Home: SetValue( Min ); return true;
            case Key::End: SetValue( Max ); return true;
            case Key::PageDown: SetValue( GetValue() - ( Max - Min ) / 10 ); return true;
            case Key::PageUp: SetValue( GetValue() + ( Max - Min ) / 10 ); return true;
            default: return false;
            }
        }
        switch ( E.Type )
        {
        case EventType::MouseDown:
            if ( E.Button != MouseButton::Left ) return false;
            SetFromMouse( E.Pos.x );
            return true;
        case EventType::MouseMove:
            if ( !IsPressed() ) return false;
            SetFromMouse( E.Pos.x );
            return true;
        case EventType::MouseUp:
            return E.Button == MouseButton::Left;
        default:
            return false;
        }
    }

    // ---- key binds: a number
    BindValue Slider::BindGet() const { return { { GetValue() } }; }
    void Slider::BindSet( const BindValue& V ) { if ( BindHas( V ) ) SetValue( float( V.V[ 0 ] ) ); }
    void Slider::BindEditor( Container& Into, BindValue& V )
    {
        auto F = std::make_shared<float>( BindHas( V ) ? float( V.V[ 0 ] ) : GetValue() );
        V.V = { *F };
        auto& S = Into.Slider( "Value", F.get(), Min, Max, Step );
        auto Show = S.OnChange;
        S.OnChange = [ F, Show, &V ]( float X ) { if ( Show ) Show( X ); V.V = { X }; };
    }
    std::string Slider::BindText( const BindValue& V ) const { return BindHas( V ) ? BindNumber( V.V[ 0 ], Step ) : ""; }
}
