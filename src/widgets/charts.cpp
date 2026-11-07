#include "tg/widgets/charts.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tg
{
    static constexpr float Pi = 3.14159265f;

    // Arc band (part of a ring) = a thick polyline along the middle radius.
    static void ArcBand( DrawList& D, Vec2 C, float R0, float R1, float A0, float A1, Color Col )
    {
        if ( A1 - A0 < 0.002f ) return;
        int Seg = std::clamp( int( ( A1 - A0 ) * ( R1 + 4 ) / 4 ), 2, 160 );
        std::vector<Vec2> P( size_t( Seg ) + 1 );
        float Rm = ( R0 + R1 ) * 0.5f;
        for ( int i = 0; i <= Seg; i++ )
        {
            float A = A0 + ( A1 - A0 ) * i / Seg;
            P[ size_t( i ) ] = { C.x + std::cos( A ) * Rm, C.y + std::sin( A ) * Rm };
        }
        D.Polyline( P.data(), int( P.size() ), Col, R1 - R0 );
    }

    static std::string Fmt( const char* F, float V )
    {
        char B[ 32 ];
        std::snprintf( B, sizeof( B ), F ? F : "%g", double( V ) );
        return B;
    }

    // ------------------------------------------------------------ BarChart

    BarChart::BarChart() { Interactive = true; }

    void BarChart::SetBars( std::vector<Bar> B )
    {
        Items = std::move( B );
        Shown.resize( Items.size(), 0 );
        Redraw();
    }

    void BarChart::SetValue( size_t i, float V )
    {
        if ( i < Items.size() ) { Items[ i ].Value = V; Redraw(); }
    }

    Vec2 BarChart::Measure( float Avail ) { return { std::min( Avail, std::max( 160.f, Items.size() * 44.f ) ), Height }; }

    float BarChart::Top() const
    {
        if ( Max > 0 ) return Max;
        float M = 0;
        for ( auto& B : Items ) M = std::max( M, B.Value );
        if ( M <= 0 ) return 1;
        // 4 grid steps of a "nice" size (1, 2, 2.5, 5 x 10^n) so the axis reads 0 / 25 / 50 / 75 / 100
        float Raw = M * 1.05f / 4, Mag = std::pow( 10.f, std::floor( std::log10( Raw ) ) );
        float Nice = Raw / Mag <= 1 ? 1 : Raw / Mag <= 2 ? 2 : Raw / Mag <= 2.5f ? 2.5f : Raw / Mag <= 5 ? 5 : 10;
        return Nice * Mag * 4;
    }

    Rect BarChart::Plot() const
    {
        const Font& S = *GetTheme().Small;
        return { Bounds.x + 36, Bounds.y + S.LineHeight + 4, Bounds.w - 36, Bounds.h - S.LineHeight * 2 - 8 };
    }

    Rect BarChart::BarRect( size_t i, float V ) const
    {
        Rect P = Plot();
        float Slot = P.w / std::max<size_t>( Items.size(), 1 );
        float W = std::min( 36.f, Slot * 0.62f );
        float H = P.h * Clamp( V / Top(), 0, 1 );
        return { P.x + Slot * i + ( Slot - W ) * 0.5f, P.Bottom() - H, W, H };
    }

    void BarChart::Draw( DrawList& D )
    {
        const Theme& T = GetTheme();
        const Font& S = *T.Small;
        Rect P = Plot();
        float Tp = Top();
        Shown.resize( Items.size(), 0 );
        for ( int g = 0; g <= 4; g++ )
        {
            float Y = std::floor( P.Bottom() - P.h * g / 4 ) + 0.5f;
            D.Line( { P.x, Y }, { P.Right(), Y }, Grid.Resolve(), 1 );
            std::string L = Fmt( Format, Tp * g / 4 );
            D.Text( S, { P.x - 8 - S.Width( L ), Y - S.LineHeight * 0.5f }, L, AxisText.Resolve() );
        }
        for ( size_t i = 0; i < Items.size(); i++ )
        {
            Animate( Shown[ i ], Items[ i ].Value, 10.f ); // bars grow to their value
            Rect R = BarRect( i, Shown[ i ] );
            Color C = Items[ i ].Color.Resolve();
            if ( int( i ) == Hover ) C = Color::Lerp( C, Color( 255, 255, 255 ), 0.2f );
            D.RectFilled( R, C, std::min( 4.f, R.w * 0.5f ), CornersTop );
            float Slot = P.w / Items.size();
            float Lw = S.Width( Items[ i ].Label );
            D.Text( S, { std::floor( P.x + Slot * i + ( Slot - Lw ) * 0.5f ), P.Bottom() + 4 }, Items[ i ].Label, AxisText.Resolve() );
            if ( ShowValues || int( i ) == Hover )
            {
                std::string V = Fmt( Format, Items[ i ].Value );
                D.Text( S, { std::floor( R.Center().x - S.Width( V ) * 0.5f ), R.y - S.LineHeight - 2 }, V, int( i ) == Hover ? T[ Tok::Text ] : T[ Tok::TextDim ] );
            }
        }
    }

    bool BarChart::OnEvent( const Event& E )
    {
        if ( E.Type == EventType::MouseLeave ) { Hover = -1; Tooltip.clear(); Redraw(); return false; }
        if ( E.Type != EventType::MouseMove ) return false;
        int H = -1;
        Rect P = Plot();
        for ( size_t i = 0; i < Items.size(); i++ )
        {
            Rect Slot = { P.x + P.w / Items.size() * i, P.y, P.w / Items.size(), P.h };
            if ( Slot.Contains( E.Pos ) ) H = int( i );
        }
        if ( H != Hover )
        {
            Hover = H;
            Tooltip = H >= 0 ? Items[ size_t( H ) ].Label + ": " + Fmt( Format, Items[ size_t( H ) ].Value ) : std::string();
            Redraw();
        }
        return false;
    }

    // ------------------------------------------------------------ DonutChart

    DonutChart::DonutChart() { Interactive = true; }

    void DonutChart::SetSlices( std::vector<Slice> S )
    {
        Items = std::move( S );
        Pop.resize( Items.size(), 0 );
        Redraw();
    }

    float DonutChart::Total() const
    {
        float T = 0;
        for ( auto& S : Items ) T += std::max( 0.f, S.Value );
        return T;
    }

    Vec2 DonutChart::Measure( float Avail )
    {
        float W = Size;
        if ( Legend )
        {
            float L = 0;
            for ( auto& S : Items ) L = std::max( L, GetTheme().Small->Width( S.Label + "  " + Fmt( Format, S.Value ) ) );
            W += 24 + 16 + L;
        }
        return { std::min( Avail, W ), std::max( Size, Legend ? Items.size() * 22.f : 0.f ) };
    }

    int DonutChart::SliceAt( Vec2 P ) const
    {
        Vec2 C = RingCenter();
        Vec2 D = P - C;
        float R = std::sqrt( D.x * D.x + D.y * D.y ), Ro = Size * 0.5f;
        if ( R < Ro - Thickness - 4 || R > Ro + 4 ) return -1;
        float A = std::atan2( D.y, D.x ) + Pi * 0.5f; // 0 at 12 o'clock
        if ( A < 0 ) A += Pi * 2;
        float T = Total(), Acc = 0;
        for ( size_t i = 0; i < Items.size() && T > 0; i++ )
        {
            Acc += std::max( 0.f, Items[ i ].Value ) / T * Pi * 2;
            if ( A <= Acc ) return int( i );
        }
        return -1;
    }

    void DonutChart::Draw( DrawList& D )
    {
        const Theme& Th = GetTheme();
        Vec2 C = RingCenter();
        float Ro = Size * 0.5f - 4, Ri = Ro - Thickness;
        float T = Total();
        Pop.resize( Items.size(), 0 );
        if ( T <= 0 ) ArcBand( D, C, Ri, Ro, 0, Pi * 2, Th[ Tok::Raised ] );
        float A = -Pi * 0.5f;
        const float Gap = Items.size() > 1 ? 0.025f : 0;
        for ( size_t i = 0; i < Items.size() && T > 0; i++ )
        {
            Animate( Pop[ i ], int( i ) == Hover ? 1.f : 0.f, 18.f );
            float Span = std::max( 0.f, Items[ i ].Value ) / T * Pi * 2;
            ArcBand( D, C, Ri, Ro + 4 * Pop[ i ], A + Gap * 0.5f, A + Span - Gap * 0.5f, Items[ i ].Color.Resolve() );
            A += Span;
        }
        // centre text
        const Font& Big = *Th.Title;
        const Font& Small = *Th.Small;
        std::string Main = !Center.empty() ? Center : Hover >= 0 ? Fmt( Format, Items[ size_t( Hover ) ].Value ) : Fmt( Format, T );
        std::string Sub = !CenterSub.empty() ? CenterSub : Hover >= 0 ? Items[ size_t( Hover ) ].Label : "total";
        float H = Big.LineHeight + Small.LineHeight;
        D.Text( Big, { std::floor( C.x - Big.Width( Main ) * 0.5f ), std::floor( C.y - H * 0.5f ) }, Main, Th[ Tok::Text ] );
        D.Text( Small, { std::floor( C.x - Small.Width( Sub ) * 0.5f ), std::floor( C.y - H * 0.5f + Big.LineHeight ) }, Sub, Th[ Tok::TextDim ] );

        if ( Legend )
        {
            float X = Bounds.x + Size + 24, Y = std::floor( Bounds.y + ( Bounds.h - Items.size() * 22.f ) * 0.5f );
            for ( size_t i = 0; i < Items.size(); i++ )
            {
                D.RectFilled( { X, Y + i * 22 + 5, 10, 10 }, Items[ i ].Color.Resolve(), 3 );
                Color Tc = int( i ) == Hover ? Th[ Tok::Text ] : Th[ Tok::TextDim ];
                D.Text( Small, { X + 16, Y + i * 22 + 10 - Small.LineHeight * 0.5f }, Items[ i ].Label, Tc );
                std::string V = Fmt( Format, Items[ i ].Value );
                D.Text( Small, { Bounds.Right() - Small.Width( V ), Y + i * 22 + 10 - Small.LineHeight * 0.5f }, V, Tc );
            }
        }
    }

    bool DonutChart::OnEvent( const Event& E )
    {
        if ( E.Type == EventType::MouseLeave ) { Hover = -1; Redraw(); return false; }
        if ( E.Type != EventType::MouseMove ) return false;
        int H = SliceAt( E.Pos );
        if ( H != Hover ) { Hover = H; Redraw(); }
        return false;
    }

    // ------------------------------------------------------------ Gauge

    Gauge::Gauge( std::string L, float V ) : Label( std::move( L ) ), Value( Clamp( V, 0, 1 ) ) {}

    void Gauge::SetValue( float V )
    {
        V = Clamp( V, 0, 1 );
        if ( V == Value ) return;
        Value = V;
        Redraw();
    }

    Vec2 Gauge::Measure( float ) { return { Size, Size * 0.86f + GetTheme().Small->LineHeight }; }

    void Gauge::Draw( DrawList& D )
    {
        const Theme& T = GetTheme();
        if ( Shown < 0 ) Shown = Value;
        Animate( Shown, Value, 8.f );
        Vec2 C = { Bounds.x + Bounds.w * 0.5f, Bounds.y + Size * 0.5f };
        float Ro = Size * 0.5f - 2, Ri = Ro - Thickness;
        const float A0 = Pi * 0.75f, Sweep = Pi * 1.5f; // 270 degrees, gap at the bottom
        ArcBand( D, C, Ri, Ro, A0, A0 + Sweep, Track.Resolve() );
        Color F = Fill.Resolve();
        if ( Thresholds ) F = Shown >= HotAt ? HotFill.Resolve() : Shown >= WarnAt ? WarnFill.Resolve() : F;
        if ( Shown > 0.002f ) ArcBand( D, C, Ri, Ro, A0, A0 + Sweep * Shown, F );

        const Font& Big = *T.Title;
        std::string V = Fmt( "%.0f", Shown * 100 ) + ( Unit ? Unit : "" );
        D.Text( Big, { std::floor( C.x - Big.Width( V ) * 0.5f ), std::floor( C.y - Big.LineHeight * 0.5f ) }, V, T[ Tok::Text ] );
        if ( !Label.empty() )
        {
            const Font& S = *T.Small;
            D.Text( S, { std::floor( C.x - S.Width( Label ) * 0.5f ), std::floor( C.y + Ro * 0.62f ) }, Label, T[ Tok::TextDim ] );
        }
    }

    // ------------------------------------------------------------ Sparkline

    Sparkline::Sparkline( size_t Cap ) : Capacity( std::max<size_t>( Cap, 2 ) ) {}

    void Sparkline::Push( float V )
    {
        Values.push_back( V );
        if ( Values.size() > Capacity ) Values.erase( Values.begin() );
        Redraw();
    }

    Vec2 Sparkline::Measure( float Avail ) { return { std::min( Avail, Width ), Height }; }

    void Sparkline::Draw( DrawList& D )
    {
        if ( Values.size() < 2 ) return;
        auto [ MnIt, MxIt ] = std::minmax_element( Values.begin(), Values.end() );
        float Lo = *MnIt, Hi = *MxIt;
        if ( Hi - Lo < 1e-6f ) { Hi += 1; Lo -= 1; }
        Rect R = Bounds.Shrink( 2 );
        std::vector<Vec2> P( Values.size() );
        for ( size_t i = 0; i < Values.size(); i++ )
            P[ i ] = { R.x + R.w * i / ( Capacity - 1 ) + R.w * float( Capacity - Values.size() ) / ( Capacity - 1 ), R.Bottom() - R.h * ( Values[ i ] - Lo ) / ( Hi - Lo ) };
        Color C = LineColor.Resolve();
        if ( Fill ) D.AreaFill( P.data(), int( P.size() ), R.Bottom(), C.WithAlpha( 60 ), C.WithAlpha( 0 ) );
        D.Polyline( P.data(), int( P.size() ), C, 1.5f );
        D.CircleFilled( P.back(), 2.5f, C );
    }
}
