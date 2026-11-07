#include "tg/widgets/progress.h"
#include "tg/draw.h"
#include <algorithm>

namespace tg
{
    void ProgressBar::SetValue( float V )
    {
        V = Clamp( V, 0, 1 );
        if ( V == Value ) return;
        Value = V;
        Redraw();
    }

    Vec2 ProgressBar::Measure( float Avail ) { return { std::min( Avail, 120.f ), Style.Height }; }

    void ProgressBar::Draw( DrawList& D )
    {
        const ProgressStyle& S = Style;
        if ( Shown < 0 ) Shown = Value;
        Animate( Shown, Value, 10.f );

        Rect Track = { Bounds.x, Bounds.y + ( Bounds.h - S.Height ) * 0.5f, Bounds.w, S.Height };
        D.RectFilled( Track, S.Track.Resolve(), S.Radius );

        Color Fill = S.Fill.Resolve();
        if ( S.Thresholds )
            Fill = Shown >= S.HotAt ? S.HotFill.Resolve() : Shown >= S.WarnAt ? S.WarnFill.Resolve() : Fill;
        float W = Track.w * Shown;
        if ( W > 0.5f ) D.RectFilled( { Track.x, Track.y, std::max( W, S.Radius * 2 ), Track.h }, Fill, S.Radius );
    }
}
