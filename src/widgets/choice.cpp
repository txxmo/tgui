#include "tg/widgets/choice.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <algorithm>
#include <cmath>

namespace tg
{
    // ------------------------------------------------------------ RadioGroup

    RadioGroup::RadioGroup( std::vector<std::string> O, int* B, Dir D ) : Options( std::move( O ) ), Bind( B ), Direction( D )
    {
        Interactive = true;
    }

    void RadioGroup::SetIndex( int i )
    {
        if ( i < 0 || i >= int( Options.size() ) || i == GetIndex() ) return;
        ( Bind ? *Bind : Own ) = i;
        if ( OnChange ) OnChange( i );
        Redraw();
    }

    Rect RadioGroup::ItemRect( size_t i ) const
    {
        const Font& F = *GetTheme().Body;
        float H = std::max( Style.Size, F.LineHeight );
        if ( Direction == Dir::Row )
        {
            float X = Bounds.x;
            for ( size_t k = 0; k < i; k++ ) X += Style.Size + Style.LabelGap + F.Width( Options[ k ] ) + Style.ItemGap * 2;
            return { X, Bounds.y, Style.Size + Style.LabelGap + F.Width( Options[ i ] ), H };
        }
        return { Bounds.x, Bounds.y + i * ( H + Style.ItemGap ), Bounds.w, H };
    }

    int RadioGroup::ItemAt( Vec2 P ) const
    {
        for ( size_t i = 0; i < Options.size(); i++ )
            if ( ItemRect( i ).Shrink( -2 ).Contains( P ) ) return int( i );
        return -1;
    }

    Vec2 RadioGroup::Measure( float )
    {
        const Font& F = *GetTheme().Body;
        float H = std::max( Style.Size, F.LineHeight );
        Vec2 Out;
        for ( auto& O : Options )
        {
            float W = Style.Size + Style.LabelGap + std::ceil( F.Width( O ) );
            if ( Direction == Dir::Row ) Out.x += W + Style.ItemGap * 2;
            else Out.x = std::max( Out.x, W );
        }
        if ( Direction == Dir::Row ) Out.y = H;
        else Out.y = Options.empty() ? 0 : Options.size() * H + ( Options.size() - 1 ) * Style.ItemGap;
        return Out;
    }

    void RadioGroup::Draw( DrawList& D )
    {
        const RadioStyle& S = Style;
        const Font& F = *GetTheme().Body;
        DotT.resize( Options.size(), 0 );
        for ( size_t i = 0; i < Options.size(); i++ )
        {
            Animate( DotT[ i ], int( i ) == GetIndex() ? 1.f : 0.f, 18.f );
            Rect R = ItemRect( i );
            Vec2 C = { R.x + S.Size * 0.5f, R.y + R.h * 0.5f };
            bool Hot = int( i ) == HoverIdx && IsHovered();
            D.CircleFilled( C, S.Size * 0.5f, S.Fill.Resolve() );
            D.CircleStroke( C, S.Size * 0.5f, Color::Lerp( ( Hot ? S.RingHover : S.Ring ).Resolve(), S.Dot.Resolve(), DotT[ i ] ), 1.5f );
            if ( DotT[ i ] > 0.01f ) D.CircleFilled( C, S.Size * 0.25f * DotT[ i ], S.Dot.Resolve() );
            Color Tc = Color::Lerp( S.TextColorOff.Resolve(), S.TextColor.Resolve(), std::max( DotT[ i ], Hot ? 1.f : 0.f ) );
            D.Text( F, { R.x + S.Size + S.LabelGap, std::floor( R.y + ( R.h - F.LineHeight ) * 0.5f ) }, Options[ i ], Tc.Fade( Enabled ? 1 : 0.5f ) );
        }
    }

    bool RadioGroup::OnEvent( const Event& E )
    {
        switch ( E.Type )
        {
        case EventType::MouseMove: { int H = ItemAt( E.Pos ); if ( H != HoverIdx ) { HoverIdx = H; Redraw(); } return false; }
        case EventType::MouseLeave: HoverIdx = -1; Redraw(); return false;
        case EventType::MouseDown: if ( E.Button != MouseButton::Left ) return false; PressIdx = ItemAt( E.Pos ); return true;
        case EventType::MouseUp:
            if ( E.Button != MouseButton::Left ) return false;
            if ( ItemAt( E.Pos ) == PressIdx && PressIdx >= 0 ) SetIndex( PressIdx );
            PressIdx = -1;
            return true;
        default: return false;
        }
    }

    // ------------------------------------------------------------ Segmented

    Segmented::Segmented( std::vector<std::string> O, int* B ) : Options( std::move( O ) ), Bind( B ) { Interactive = true; }

    void Segmented::SetIndex( int i )
    {
        if ( i < 0 || i >= int( Options.size() ) || i == GetIndex() ) return;
        ( Bind ? *Bind : Own ) = i;
        if ( OnChange ) OnChange( i );
        Redraw();
    }

    Rect Segmented::SegmentRect( size_t i ) const
    {
        Rect In = Bounds.Shrink( Style.Inset );
        float W = Options.empty() ? In.w : In.w / Options.size();
        return { std::floor( In.x + W * i ), In.y, std::floor( W ), In.h };
    }

    Vec2 Segmented::Measure( float )
    {
        const Font& F = *GetTheme().Bold;
        float Seg = Style.MinSegment;
        for ( auto& O : Options ) Seg = std::max( Seg, std::ceil( F.Width( O ) ) + 24 );
        return { Seg * Options.size() + Style.Inset * 2, Style.Height };
    }

    void Segmented::Draw( DrawList& D )
    {
        const SegmentedStyle& S = Style;
        const Font& F = *GetTheme().Bold;
        D.RectFilled( Bounds, S.Fill.Resolve(), S.Radius );
        D.RectStroke( Bounds, S.Stroke.Resolve(), S.Radius );
        if ( Options.empty() ) return;
        int Cur = std::clamp( GetIndex(), 0, int( Options.size() ) - 1 );
        Rect A = SegmentRect( size_t( Cur ) );
        if ( SlideX < 0 ) SlideX = A.x;
        Animate( SlideX, A.x, 20.f );
        D.RectFilled( { SlideX, A.y, A.w, A.h }, S.Active.Resolve(), S.Radius - 2 );
        for ( size_t i = 0; i < Options.size(); i++ )
        {
            Rect R = SegmentRect( i );
            float W = F.Width( Options[ i ] );
            D.Text( F, { std::floor( R.x + ( R.w - W ) * 0.5f ), std::floor( R.y + ( R.h - F.LineHeight ) * 0.5f ) }, Options[ i ],
                    ( int( i ) == Cur ? S.ActiveText : S.Text ).Resolve() );
        }
    }

    bool Segmented::OnEvent( const Event& E )
    {
        auto At = [ & ]( Vec2 P ) { for ( size_t i = 0; i < Options.size(); i++ ) if ( SegmentRect( i ).Contains( P ) ) return int( i ); return -1; };
        if ( E.Button != MouseButton::Left ) return false;
        if ( E.Type == EventType::MouseDown ) { PressIdx = At( E.Pos ); return true; }
        if ( E.Type == EventType::MouseUp )
        {
            if ( At( E.Pos ) == PressIdx && PressIdx >= 0 ) SetIndex( PressIdx );
            PressIdx = -1;
            return true;
        }
        return false;
    }
}
