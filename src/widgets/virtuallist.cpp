#include "tg/widgets/virtuallist.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    VirtualList::VirtualList( size_t Count, float Rh ) : RowHeight( Rh ), N( Count )
    {
        Interactive = true;
        Focusable = true;
        Layout.Width = Grow();
        Layout.Height = Grow();
    }

    void VirtualList::SetCount( size_t C )
    {
        bool Follow = StickToBottom && AtBottom;
        N = C;
        if ( HeightOf ) Rebuild();
        if ( Selected >= int( N ) ) Selected = -1;
        if ( Follow ) ScrollToBottom();
        Target = std::clamp( Target, 0.f, MaxScroll() );
        Redraw();
    }

    void VirtualList::InvalidateHeights() { CachedWidth = -1; Rebuild(); Redraw(); }

    void VirtualList::Rebuild()
    {
        if ( !HeightOf ) { Offsets.clear(); return; }
        float W = Bounds.w - 8;
        CachedWidth = Bounds.w;
        Offsets.resize( N + 1 );
        Offsets[ 0 ] = 0;
        for ( size_t i = 0; i < N; i++ ) Offsets[ i + 1 ] = Offsets[ i ] + std::max( 1.f, HeightOf( i, W ) );
    }

    float VirtualList::Top( size_t R ) const { return HeightOf && R < Offsets.size() ? Offsets[ R ] : R * RowHeight; }
    float VirtualList::Height( size_t R ) const { return HeightOf && R + 1 < Offsets.size() ? Offsets[ R + 1 ] - Offsets[ R ] : RowHeight; }
    float VirtualList::Total() const { return HeightOf ? ( Offsets.empty() ? 0 : Offsets.back() ) : N * RowHeight; }
    float VirtualList::MaxScroll() const { return std::max( 0.f, Total() - Bounds.h ); }

    Rect VirtualList::RowRect( size_t R ) const { return { Bounds.x, Bounds.y + Top( R ) - Scroll, Bounds.w - 8, Height( R ) }; }

    int VirtualList::RowAt( Vec2 P ) const
    {
        if ( !Bounds.Contains( P ) || P.x > Bounds.Right() - 8 ) return -1;
        float Y = P.y - Bounds.y + Scroll;
        size_t R;
        if ( HeightOf )
        {
            auto It = std::upper_bound( Offsets.begin(), Offsets.end(), Y );
            if ( It == Offsets.begin() ) return -1;
            R = size_t( It - Offsets.begin() ) - 1;
        }
        else R = size_t( std::max( 0.f, Y ) / RowHeight );
        return R < N ? int( R ) : -1;
    }

    void VirtualList::ScrollTo( size_t R )
    {
        if ( R >= N ) return;
        float T = Top( R ), H = Height( R );
        if ( T < Target ) Target = T;
        else if ( T + H > Target + Bounds.h ) Target = T + H - Bounds.h;
        Target = std::clamp( Target, 0.f, MaxScroll() );
        Redraw();
    }

    void VirtualList::ScrollToBottom()
    {
        Target = MaxScroll();
        AtBottom = true;
        Redraw();
    }

    Vec2 VirtualList::Measure( float Avail ) { return { std::min( Avail, 320.f ), std::min( 400.f, std::max( 60.f, Total() ) ) }; }

    void VirtualList::Arrange( Rect R )
    {
        bool Follow = StickToBottom && AtBottom;
        Bounds = R;
        if ( HeightOf && R.w != CachedWidth ) Rebuild();
        if ( Follow ) Target = Scroll = MaxScroll();
        Target = std::clamp( Target, 0.f, MaxScroll() );
        Scroll = std::clamp( Scroll, 0.f, MaxScroll() );
    }

    Rect VirtualList::BarRect() const
    {
        float T = Total();
        if ( T <= Bounds.h ) return {};
        float H = std::max( 24.f, Bounds.h * Bounds.h / T );
        float Y = Bounds.y + ( Bounds.h - H ) * ( Scroll / std::max( 1.f, MaxScroll() ) );
        return { Bounds.Right() - 6, Y, 4, H };
    }

    void VirtualList::Draw( DrawList& D )
    {
        Animate( Scroll, Target, 22.f );
        AtBottom = Target >= MaxScroll() - 1;
        const VirtualListStyle& S = Style;
        if ( S.Fill.Resolve().a ) D.RectFilled( Bounds, S.Fill.Resolve(), S.Radius );
        D.PushClip( Bounds );
        if ( N )
        {
            // first visible row by binary search (variable) or division (fixed)
            size_t First;
            if ( HeightOf )
            {
                auto It = std::upper_bound( Offsets.begin(), Offsets.end(), Scroll );
                First = It == Offsets.begin() ? 0 : size_t( It - Offsets.begin() ) - 1;
            }
            else First = size_t( Scroll / RowHeight );
            const Font& F = *GetTheme().Body;
            for ( size_t i = First; i < N; i++ )
            {
                Rect R = RowRect( i );
                if ( R.y > Bounds.Bottom() ) break;
                bool Hv = int( i ) == Hover && IsHovered(), Sel = Selectable && int( i ) == Selected;
                if ( Sel ) D.RectFilled( R, S.SelectedFill.Resolve(), S.Radius );
                else if ( Hv ) D.RectFilled( R, S.HoverFill.Resolve(), S.Radius );
                if ( DrawRow ) DrawRow( D, i, R, Hv, Sel );
                else if ( TextOf )
                {
                    Rect In = R.Shrink( S.RowPadding );
                    D.Text( F, { In.x, std::floor( In.y + ( In.h - F.LineHeight ) * 0.5f ) }, TextOf( i ), S.TextColor.Resolve() );
                }
            }
        }
        D.PopClip();
        Rect B = BarRect();
        if ( !B.Empty() ) D.RectFilled( B, S.Scrollbar.Resolve().Fade( DragBar || IsHovered() ? 1.f : 0.6f ), 2 );
    }

    bool VirtualList::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseWheel:
            if ( MaxScroll() <= 0 ) return false;
            Target = std::clamp( Target - E.Wheel.y * 60, 0.f, MaxScroll() );
            Redraw();
            return true;
        case EventType::MouseMove:
        {
            if ( DragBar )
            {
                float Track = Bounds.h - BarRect().h;
                float T = ( E.Pos.y - DragGrab - Bounds.y ) / std::max( 1.f, Track );
                Target = Scroll = std::clamp( T, 0.f, 1.f ) * MaxScroll();
                Redraw();
                return true;
            }
            int H = RowAt( E.Pos );
            if ( H != Hover ) { Hover = H; Redraw(); }
            return false;
        }
        case EventType::MouseLeave: Hover = -1; Redraw(); return false;
        case EventType::MouseDown:
        {
            if ( E.Button != MouseButton::Left ) return false;
            Rect B = BarRect();
            if ( !B.Empty() && E.Pos.x >= Bounds.Right() - 10 )
            {
                DragBar = true;
                DragGrab = B.Contains( E.Pos ) ? E.Pos.y - B.y : B.h * 0.5f;
                return true;
            }
            int R = RowAt( E.Pos );
            if ( R < 0 ) return true;
            if ( Selectable ) Selected = R;
            if ( E.Clicks == 2 && OnActivate ) OnActivate( size_t( R ) );
            else if ( OnClick ) OnClick( size_t( R ) );
            Redraw();
            return true;
        }
        case EventType::MouseUp: DragBar = false; return E.Button == MouseButton::Left;
        case EventType::KeyDown:
        {
            if ( !N ) return false;
            int Page = std::max( 1, int( Bounds.h / std::max( 1.f, Height( 0 ) ) ) - 1 );
            int S = Selected;
            switch ( E.KeyCode )
            {
            case Key::Down: S = std::min( int( N ) - 1, S + 1 ); break;
            case Key::Up: S = std::max( 0, S - 1 ); break;
            case Key::PageDown: S = std::min( int( N ) - 1, S + Page ); break;
            case Key::PageUp: S = std::max( 0, S - Page ); break;
            case Key::Home: S = 0; break;
            case Key::End: S = int( N ) - 1; break;
            case Key::Enter: if ( Selected >= 0 && OnActivate ) OnActivate( size_t( Selected ) ); return Selected >= 0;
            default: return false;
            }
            if ( Selectable ) Selected = S;
            ScrollTo( size_t( S ) );
            if ( OnClick ) OnClick( size_t( S ) );
            return true;
        }
        default: return false;
        }
    }
}
