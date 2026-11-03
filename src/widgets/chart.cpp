#include "tg/widgets/chart.h"
#include "tg/draw.h"
#include "tg/font.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tg
{
    LineChart::LineChart( size_t Cap ) : Capacity( std::max<size_t>( Cap, 2 ) ) {}

    void LineChart::Push( float V )
    {
        Values.push_back( V );
        if ( Values.size() > Capacity ) Values.erase( Values.begin() );
        Redraw();
    }

    void LineChart::SetData( std::vector<float> V )
    {
        Values = std::move( V );
        if ( Values.size() > Capacity ) Values.erase( Values.begin(), Values.end() - Capacity );
        Redraw();
    }

    Vec2 LineChart::Measure( float Avail ) { return { std::min( Avail, 240.f ), Style.Height }; }

    void LineChart::Draw( DrawList& D )
    {
        const ChartStyle& S = Style;
        const Font& F = *GetTheme().Small;

        float Lo = Min, Hi = Max;
        if ( AutoRange && !Values.empty() )
        {
            auto [ MnIt, MxIt ] = std::minmax_element( Values.begin(), Values.end() );
            Lo = std::min( 0.f, *MnIt );
            Hi = *MxIt <= Lo ? Lo + 1 : *MxIt * 1.15f;
        }
        if ( Hi <= Lo ) Hi = Lo + 1;

        const float AxisW = 40;
        Rect Plot = { Bounds.x + AxisW, Bounds.y + F.LineHeight * 0.5f, Bounds.w - AxisW, Bounds.h - F.LineHeight };

        // grid + axis labels
        char Buf[ 32 ];
        for ( int i = 0; i <= S.GridLines; i++ )
        {
            float t = float( i ) / S.GridLines;
            float Y = std::floor( Plot.Bottom() - Plot.h * t ) + 0.5f;
            D.Line( { Plot.x, Y }, { Plot.Right(), Y }, S.Grid.Resolve(), 1 );
            std::snprintf( Buf, sizeof( Buf ), "%g%s", std::round( ( Lo + ( Hi - Lo ) * t ) * 10 ) / 10, Unit );
            D.Text( F, { Bounds.x + AxisW - 8 - F.Width( Buf ), Y - F.LineHeight * 0.5f }, Buf, S.AxisText.Resolve() );
        }
        if ( Values.size() < 2 ) return;

        // points: newest at the right edge, history scrolls left
        std::vector<Vec2> Pts( Values.size() );
        float Dx = Plot.w / float( Capacity - 1 );
        float X0 = Plot.Right() - Dx * float( Values.size() - 1 );
        for ( size_t i = 0; i < Values.size(); i++ )
        {
            float t = Clamp( ( Values[ i ] - Lo ) / ( Hi - Lo ), 0, 1 );
            Pts[ i ] = { X0 + Dx * i, Plot.Bottom() - Plot.h * t };
        }

        Color Lc = S.Line.Resolve();
        D.PushClip( Plot.Shrink( -2 ) );
        if ( S.FillAlpha ) D.AreaFill( Pts.data(), int( Pts.size() ), Plot.Bottom(), Lc.WithAlpha( S.FillAlpha ), Lc.WithAlpha( 0 ) );
        D.Polyline( Pts.data(), int( Pts.size() ), Lc, S.LineWidth );
        D.CircleFilled( Pts.back(), 3.5f, Lc );
        D.PopClip();
    }
}
