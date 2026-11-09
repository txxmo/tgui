#include "tg/widgets/tabview.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/widgets/container.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    TabStyle& TabView::Defaults()
    {
        static TabStyle S;
        return S;
    }

    TabView::TabView() : Style( Defaults() )
    {
        Interactive = true;
        Layout.Height = Grow();
    }

    Container& TabView::AddTab( std::string Name )
    {
        Names.push_back( std::move( Name ) );
        auto& P = Add<Container>( Dir::Column, 10 );
        P.Visible = Pages.empty();
        Pages.push_back( &P );
        if ( Bind && *Bind == int( Pages.size() - 1 ) ) Select( *Bind );
        return P;
    }

    void TabView::Select( int i )
    {
        if ( i < 0 || i >= int( Pages.size() ) || i == Active ) return;
        if ( Prev >= 0 && Prev != i ) Pages[ Prev ]->Visible = false; // a fade was still running
        Prev = Style.FadeSpeed > 0 ? Active : -1;
        if ( Prev < 0 ) Pages[ Active ]->Visible = false;
        Active = i;
        Pages[ i ]->Visible = true;
        FadeT = Style.FadeSpeed > 0 ? 0.f : 1.f;
        if ( Bind ) *Bind = i;
        if ( OnChange ) OnChange( i );
        Invalidate();
    }

    Rect TabView::TabRect( size_t i ) const
    {
        Rect In = Strip.Shrink( Style.Inset );
        if ( Names.empty() ) return In;
        if ( Style.EqualWidth )
        {
            float W = In.w / float( Names.size() );
            return { std::floor( In.x + W * i ), In.y, std::floor( W ), In.h };
        }
        const Font& F = *GetTheme().Bold;
        float X = In.x;
        for ( size_t k = 0; k < i; k++ ) X += F.Width( Names[ k ] ) + 32;
        return { X, In.y, F.Width( Names[ i ] ) + 32, In.h };
    }

    int TabView::TabAt( Vec2 P ) const
    {
        if ( !Strip.Contains( P ) ) return -1;
        for ( size_t i = 0; i < Names.size(); i++ )
            if ( TabRect( i ).Contains( P ) ) return int( i );
        return -1;
    }

    Vec2 TabView::Measure( float Avail )
    {
        Vec2 P = Pages.empty() ? Vec2{} : Pages[ Active ]->MeasureCached( Avail );
        float StripW = 0;
        for ( auto& N : Names ) StripW += GetTheme().Bold->Width( N ) + 32;
        return { std::max( P.x, StripW ), Style.Height + Style.Gap + P.y };
    }

    void TabView::Arrange( Rect R )
    {
        Bounds = R;
        if ( Bind && *Bind != Active ) Select( *Bind ); // follow the bound int
        Strip = { R.x, R.y, R.w, Style.Height };
        PageR = { R.x, R.y + Style.Height + Style.Gap, R.w, std::max( 0.f, R.h - Style.Height - Style.Gap ) };
        for ( size_t i = 0; i < Pages.size(); i++ )
            if ( Pages[ i ]->Visible ) Pages[ i ]->Arrange( PageR );
    }

    void TabView::Draw( DrawList& D )
    {
        const TabStyle& S = Style;
        const Font& F = *GetTheme().Bold;

        // ---- strip
        D.RectFilled( Strip, S.StripFill.Resolve(), S.Radius );
        D.RectStroke( Strip, S.StripStroke.Resolve(), S.Radius );

        // the active highlight slides between tabs
        Rect A = TabRect( size_t( Active ) );
        // animated in local coordinates: when the whole tab view moves (window drag, scroll), the
        // highlight moves with it instead of trailing behind
        float Lx = A.x - Bounds.x;
        if ( SlideX < 0 ) SlideX = Lx;
        Animate( SlideX, Lx, 18.f );
        Rect Hl = { Bounds.x + SlideX, A.y, A.w, A.h };
        D.RectFilled( Hl, S.ActiveFill.Resolve(), S.Radius - 2 );
        D.RectFilled( { Hl.x + 10, Hl.Bottom() - 2, Hl.w - 20, 2 }, S.Underline.Resolve(), 1 );

        for ( size_t i = 0; i < Names.size(); i++ )
        {
            Rect T = TabRect( i );
            if ( int( i ) == HoverTab && int( i ) != Active ) D.RectFilled( T, S.HoverFill.Resolve(), S.Radius - 2 );
            std::string Name = Ellipsize( F, Names[ i ], T.w - 12 ); // narrow windows: "More wid…"
            float W = F.Width( Name );
            D.PushClip( T );
            D.Text( F, { std::floor( T.x + ( T.w - W ) * 0.5f ), std::floor( T.y + ( T.h - F.LineHeight ) * 0.5f ) }, Name,
                    ( int( i ) == Active ? S.ActiveText : S.Text ).Resolve() );
            D.PopClip();
        }

        // ---- pages: cross-fade old -> new
        if ( FadeT < 1 ) Animate( FadeT, 1.f, S.FadeSpeed );
        if ( FadeT >= 1 && Prev >= 0 ) { Pages[ Prev ]->Visible = false; Prev = -1; }
        if ( Prev >= 0 )
        {
            size_t V0 = D.Vtx.size();
            Pages[ Prev ]->Draw( D );
            D.FadeFrom( V0, 1 - FadeT );
        }
        if ( !Pages.empty() )
        {
            size_t V0 = D.Vtx.size();
            Pages[ Active ]->Draw( D );
            D.FadeFrom( V0, FadeT );
        }
    }

    Widget* TabView::HitTest( Vec2 P )
    {
        if ( !Visible || !Bounds.Contains( P ) ) return nullptr;
        if ( Strip.Contains( P ) ) return this;
        if ( !Pages.empty() )
            if ( Widget* H = Pages[ Active ]->HitTest( P ) ) return H; // never the page that's fading out
        return nullptr;
    }

    Cursor TabView::GetCursor() const { return HoverTab >= 0 && HoverTab != Active ? Cursor::Hand : Cursor::Arrow; }

    bool TabView::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseMove:
        {
            int H = TabAt( E.Pos );
            if ( H != HoverTab ) { HoverTab = H; Redraw(); }
            return false;
        }
        case EventType::MouseLeave:
            HoverTab = -1;
            Redraw();
            return false;
        case EventType::MouseDown:
            if ( E.Button != MouseButton::Left || !Strip.Contains( E.Pos ) ) return false;
            PressTab = TabAt( E.Pos );
            return true;
        case EventType::MouseUp:
        {
            if ( E.Button != MouseButton::Left || PressTab < 0 ) return false;
            int T = TabAt( E.Pos );
            if ( T == PressTab ) Select( T );
            PressTab = -1;
            return true;
        }
        default:
            return false;
        }
    }
}
