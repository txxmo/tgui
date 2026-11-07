#include "tg/widgets/collapsible.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    Collapsible::Collapsible( std::string T, bool* O ) : Container( Dir::Column, 8, Insets( 12, 10 ) ), Title( std::move( T ) ), Bind( O )
    {
        Interactive = true;
        OpenT = IsOpen() ? 1.f : 0.f;
    }

    void Collapsible::SetOpen( bool O )
    {
        if ( O == IsOpen() ) return;
        ( Bind ? *Bind : Own ) = O;
        if ( OnToggle ) OnToggle( O );
        Invalidate();
    }

    Vec2 Collapsible::Measure( float Avail )
    {
        Vec2 Body = MeasureChildren( std::max( 0.f, Avail - Layout.Padding.Horizontal() ) );
        float BodyH = ( Body.y + Layout.Padding.Vertical() ) * OpenT;
        return { std::max( Body.x + Layout.Padding.Horizontal(), GetTheme().Bold->Width( Title ) + 48 ), Frame.HeaderHeight + BodyH };
    }

    void Collapsible::Arrange( Rect R )
    {
        Bounds = R;
        HeaderR = { R.x, R.y, R.w, Frame.HeaderHeight };
        // children get their full height; Draw clips them to the animated height
        float InnerW = R.w - Layout.Padding.Horizontal();
        float FullH = MeasureChildren( InnerW ).y;
        ArrangeChildren( { R.x + Layout.Padding.Left, R.y + Frame.HeaderHeight + Layout.Padding.Top, InnerW, FullH } );
    }

    void Collapsible::Draw( DrawList& D )
    {
        const CollapsibleStyle& S = Frame;
        const Font& F = *GetTheme().Bold;
        float Before = OpenT;
        Animate( OpenT, IsOpen() ? 1.f : 0.f, 16.f );
        if ( OpenT != Before ) Invalidate(); // height is animating: let the parent re-lay out
        Animate( HoverT, OverHeader && IsHovered() ? 1.f : 0.f );

        D.RectFilled( Bounds, GetTheme()[ Tok::Surface ], S.Radius );
        bool Folded = OpenT < 0.01f;
        D.RectFilled( HeaderR, S.HeaderFill.Resolve(), S.Radius, Folded ? CornersAll : CornersTop );
        D.RectFilled( HeaderR, S.HeaderHover.Resolve().Fade( HoverT ), S.Radius, Folded ? CornersAll : CornersTop );

        // chevron rotates from > to v
        Vec2 C = { HeaderR.x + 16, HeaderR.y + HeaderR.h * 0.5f };
        float A = OpenT * 1.5707963f;
        auto Rot = [ & ]( float X, float Y ) { return Vec2{ C.x + X * std::cos( A ) - Y * std::sin( A ), C.y + X * std::sin( A ) + Y * std::cos( A ) }; };
        Vec2 P[ 3 ] = { Rot( -2, -4 ), Rot( 2, 0 ), Rot( -2, 4 ) };
        D.Polyline( P, 3, S.Chevron.Resolve(), 1.6f );
        D.Text( F, { HeaderR.x + 30, std::floor( HeaderR.y + ( HeaderR.h - F.LineHeight ) * 0.5f ) }, Title, S.Title.Resolve() );

        if ( !Folded )
        {
            D.RectFilled( { HeaderR.x, HeaderR.Bottom() - 1, HeaderR.w, 1 }, S.Stroke.Resolve() );
            size_t V0 = D.Vtx.size();
            D.PushClip( { Bounds.x, HeaderR.Bottom(), Bounds.w, Bounds.h - HeaderR.h } );
            DrawChildren( D );
            D.PopClip();
            D.FadeFrom( V0, OpenT );
        }
        D.RectStroke( Bounds, S.Stroke.Resolve(), S.Radius );
    }

    Widget* Collapsible::HitTest( Vec2 P )
    {
        if ( !Visible || !Bounds.Contains( P ) ) return nullptr;
        if ( HeaderR.Contains( P ) ) return this;
        if ( OpenT > 0.99f )
            if ( Widget* H = Widget::HitTest( P ) ) return H != this ? H : this;
        return this;
    }

    bool Collapsible::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseMove: { bool O = HeaderR.Contains( E.Pos ); if ( O != OverHeader ) { OverHeader = O; Redraw(); } return false; }
        case EventType::MouseLeave: OverHeader = false; Redraw(); return false;
        case EventType::MouseDown: return E.Button == MouseButton::Left && HeaderR.Contains( E.Pos );
        case EventType::MouseUp:
            if ( E.Button != MouseButton::Left || !HeaderR.Contains( E.Pos ) ) return false;
            SetOpen( !IsOpen() );
            return true;
        default: return false;
        }
    }
}
