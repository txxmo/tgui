#include "tg/widgets/scrollview.h"
#include "tg/context.h"
#include "tg/draw.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    ScrollView::ScrollView()
    {
        Interactive = true;
        Layout.Height = Grow();
        Add<Widget>(); // content column
    }

    float ScrollView::MaxScroll() const { return std::max( 0.f, ContentH - Bounds.h ); }

    void ScrollView::ScrollTo( float Y )
    {
        Target = Clamp( Y, 0, MaxScroll() );
        AtBottom = Target >= MaxScroll() - 1;
        Redraw();
    }

    void ScrollView::ScrollToBottom()
    {
        AtBottom = true;
        Target = MaxScroll();
        Redraw();
    }

    Vec2 ScrollView::Measure( float Avail )
    {
        const Insets& P = Layout.Padding;
        Vec2 C = Content().MeasureCached( Avail - P.Horizontal() );
        float H = C.y + P.Vertical();
        if ( MaxHeight > 0 && H > MaxHeight ) { H = MaxHeight; C.x += Style.BarWidth + Style.BarMargin; }
        return { C.x + P.Horizontal(), H };
    }

    void ScrollView::Arrange( Rect R )
    {
        Bounds = R;
        Rect In = R.Shrink( Layout.Padding );
        Widget& C = Content();

        // Reserve room for the scrollbar only when the content actually overflows.
        float W = In.w;
        ContentH = C.MeasureCached( W ).y + Layout.Padding.Vertical();
        if ( ContentH > R.h )
        {
            W = In.w - ( Style.BarWidth + Style.BarMargin );
            ContentH = C.MeasureCached( W ).y + Layout.Padding.Vertical();
        }

        if ( StickToBottom && AtBottom )
        {
            Target = MaxScroll();
            if ( !SmoothStick ) Scroll = Target;
        }
        Target = Clamp( Target, 0, MaxScroll() );
        Scroll = Clamp( Scroll, 0, MaxScroll() );

        C.Arrange( { In.x, In.y - std::round( Scroll ), W, std::max( ContentH - Layout.Padding.Vertical(), In.h ) } );
    }

    Rect ScrollView::ThumbRect() const
    {
        float Track = Bounds.h - Style.BarMargin * 2;
        float H = std::max( Style.MinThumb, Track * Bounds.h / std::max( ContentH, 1.f ) );
        float T = MaxScroll() > 0 ? Scroll / MaxScroll() : 0;
        return { Bounds.Right() - Style.BarMargin - Style.BarWidth, Bounds.y + Style.BarMargin + ( Track - H ) * T, Style.BarWidth, H };
    }

    void ScrollView::Draw( DrawList& D )
    {
        if ( Scroll != Target )
        {
            Animate( Scroll, Target, 22.f );
            Arrange( Bounds ); // cheap: measures are cached, we just shift the content
        }

        D.PushClip( Bounds );
        DrawChildren( D );
        D.PopClip();

        if ( ContentH > Bounds.h + 0.5f )
        {
            Rect T = ThumbRect();
            Context* C = GetContext();
            bool Hot = DraggingThumb || ( C && T.Shrink( -4 ).Contains( C->MousePos() ) );
            D.RectFilled( T, ( Hot ? Style.ThumbHover : Style.Thumb ).Resolve(), Style.BarWidth * 0.5f );
        }
    }

    Widget* ScrollView::HitTest( Vec2 P )
    {
        if ( !Visible || !Bounds.Contains( P ) ) return nullptr;
        if ( ContentH > Bounds.h && ThumbRect().Shrink( -3 ).Contains( P ) ) return this;
        if ( Widget* H = Content().HitTest( P ) ) return H;
        return this;
    }

    bool ScrollView::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseWheel:
            if ( MaxScroll() <= 0 ) return false; // let an outer ScrollView have it
            ScrollTo( Target - E.Wheel.y * Style.WheelStep );
            return true;
        case EventType::MouseDown:
        {
            Rect T = ThumbRect();
            if ( E.Button != MouseButton::Left || ContentH <= Bounds.h || !T.Shrink( -3 ).Contains( E.Pos ) ) return false;
            DraggingThumb = true;
            DragOffset = E.Pos.y - T.y;
            return true;
        }
        case EventType::MouseMove:
        {
            if ( !DraggingThumb ) return false;
            float Track = Bounds.h - Style.BarMargin * 2 - ThumbRect().h;
            float t = Track > 0 ? Clamp( ( E.Pos.y - DragOffset - Bounds.y - Style.BarMargin ) / Track, 0, 1 ) : 0;
            Scroll = Target = t * MaxScroll();
            AtBottom = Target >= MaxScroll() - 1;
            Arrange( Bounds );
            return true;
        }
        case EventType::MouseUp:
            if ( !DraggingThumb ) return false;
            DraggingThumb = false;
            return true;
        default:
            return false;
        }
    }
}
