#include "tg/widgets/rangeslider.h"
#include "tg/draw.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    RangeSlider::RangeSlider( float* L, float* H, float Mn, float Mx, float St ) : Low( L ), High( H ), Min( Mn ), Max( Mx ), Step( St ), OwnLo( Mn ), OwnHi( Mx )
    {
        Interactive = true;
    }

    float RangeSlider::ToX( float V ) const
    {
        float L = Bounds.x + Style.KnobRadius, W = Bounds.w - Style.KnobRadius * 2;
        return L + W * ( Max != Min ? Clamp( ( V - Min ) / ( Max - Min ), 0, 1 ) : 0 );
    }

    float RangeSlider::ToValue( float X ) const
    {
        float L = Bounds.x + Style.KnobRadius, W = Bounds.w - Style.KnobRadius * 2;
        float V = Min + ( Max - Min ) * ( W > 0 ? Clamp( ( X - L ) / W, 0, 1 ) : 0 );
        if ( Step > 0 ) V = Min + std::round( ( V - Min ) / Step ) * Step;
        return V;
    }

    void RangeSlider::SetRange( float Lo, float Hi )
    {
        Lo = Clamp( Lo, Min, Max );
        Hi = Clamp( Hi, Min, Max );
        if ( Lo > Hi ) std::swap( Lo, Hi );
        if ( Lo == GetLow() && Hi == GetHigh() ) return;
        ( Low ? *Low : OwnLo ) = Lo;
        ( High ? *High : OwnHi ) = Hi;
        if ( OnChange ) OnChange( Lo, Hi );
        Redraw();
    }

    Vec2 RangeSlider::Measure( float Avail ) { return { std::min( Avail, 160.f ), Style.Height }; }

    void RangeSlider::Draw( DrawList& D )
    {
        const SliderStyle& S = Style;
        Animate( HoverT, IsHovered() || IsPressed() ? 1.f : 0.f );
        float Cy = Bounds.y + Bounds.h * 0.5f;
        float X0 = Bounds.x + S.KnobRadius, X1 = Bounds.Right() - S.KnobRadius;
        D.RectFilled( { X0, Cy - S.TrackHeight * 0.5f, X1 - X0, S.TrackHeight }, S.Track.Resolve(), S.TrackHeight * 0.5f );
        float A = ToX( GetLow() ), B = ToX( GetHigh() );
        D.RectFilled( { A, Cy - S.TrackHeight * 0.5f, B - A, S.TrackHeight }, S.TrackFill.Resolve(), S.TrackHeight * 0.5f );
        for ( int K = 1; K <= 2; K++ )
        {
            Vec2 C = { K == 1 ? A : B, Cy };
            float R = S.KnobRadius + ( Dragging == K ? 1.5f : HoverT * 0.75f );
            D.CircleFilled( C, R, S.KnobStroke.Resolve() );
            D.CircleFilled( C, R - 2, S.Knob.Resolve() );
        }
    }

    bool RangeSlider::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseDown:
        {
            if ( E.Button != MouseButton::Left ) return false;
            float A = ToX( GetLow() ), B = ToX( GetHigh() );
            // grab the nearer knob; when they overlap, pick by drag direction side
            Dragging = std::fabs( E.Pos.x - A ) < std::fabs( E.Pos.x - B ) || ( A == B && E.Pos.x < A ) ? 1 : 2;
            [[fallthrough]];
        }
        case EventType::MouseMove:
        {
            if ( !Dragging || !IsPressed() ) return false;
            float V = ToValue( E.Pos.x );
            if ( Dragging == 1 ) SetRange( std::min( V, GetHigh() ), GetHigh() );
            else SetRange( GetLow(), std::max( V, GetLow() ) );
            return true;
        }
        case EventType::MouseUp:
            Dragging = 0;
            Redraw();
            return E.Button == MouseButton::Left;
        default:
            return false;
        }
    }
}
