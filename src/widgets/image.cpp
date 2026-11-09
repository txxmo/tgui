#include "tg/widgets/image.h"
#include "tg/draw.h"
#include <algorithm>

namespace tg
{
    void Image::SetSource( TextureId T, int W, int H )
    {
        bool Resized = W != TexW || H != TexH;
        Tex = T;
        TexW = W;
        TexH = H;
        if ( Resized ) Invalidate();
        else Redraw();
    }

    Vec2 Image::Measure( float Avail )
    {
        if ( TexW <= 0 || TexH <= 0 ) return {};
        float W = std::min( Avail, float( TexW ) );
        return { W, W * TexH / TexW };
    }

    void Image::Draw( DrawList& D )
    {
        const ImageStyle& S = Style;
        if ( !Tex || TexW <= 0 || TexH <= 0 ) return;
        Rect Dst = Bounds;
        Vec2 UV0 = { 0, 0 }, UV1 = { 1, 1 };
        float SrcAspect = float( TexW ) / TexH, DstAspect = Bounds.w / std::max( 1.f, Bounds.h );

        if ( S.Mode == ImageFit::Contain )
        {
            D.RectFilled( Bounds, S.Letterbox.Resolve(), S.Radius );
            if ( SrcAspect > DstAspect ) { Dst.h = Bounds.w / SrcAspect; Dst.y += ( Bounds.h - Dst.h ) * 0.5f; }
            else { Dst.w = Bounds.h * SrcAspect; Dst.x += ( Bounds.w - Dst.w ) * 0.5f; }
        }
        else if ( S.Mode == ImageFit::Cover )
        {
            if ( SrcAspect > DstAspect ) { float k = DstAspect / SrcAspect; UV0.x = ( 1 - k ) * 0.5f; UV1.x = 1 - UV0.x; }
            else { float k = SrcAspect / DstAspect; UV0.y = ( 1 - k ) * 0.5f; UV1.y = 1 - UV0.y; }
        }
        D.Image( Tex, Dst, UV0, UV1, S.Tint, S.Radius );
    }
}
