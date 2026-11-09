#include "tg/svg.h"
#include "tg/context.h"
#include "tg/draw.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#if defined( __GNUC__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#include "nanosvg.h"
#include "nanosvgrast.h"
#if defined( __GNUC__ )
#pragma GCC diagnostic pop
#endif

namespace tg
{
    static NSVGimage* Parse( const std::string& S )
    {
        std::vector<char> Buf( S.begin(), S.end() );
        Buf.push_back( 0 ); // nanosvg parses in place
        return nsvgParse( Buf.data(), "px", 96.f );
    }

    bool SvgSize( const std::string& Svg, float& W, float& H )
    {
        NSVGimage* I = Parse( Svg );
        if ( !I ) return false;
        W = I->width;
        H = I->height;
        nsvgDelete( I );
        return W > 0 && H > 0;
    }

    std::vector<uint32_t> RasterizeSvgPixels( const std::string& Svg, int W, int H )
    {
        std::vector<uint32_t> Out;
        if ( W <= 0 || H <= 0 ) return Out;
        NSVGimage* I = Parse( Svg );
        if ( !I || I->width <= 0 || I->height <= 0 ) { if ( I ) nsvgDelete( I ); return Out; }
        NSVGrasterizer* R = nsvgCreateRasterizer();
        float S = std::min( W / I->width, H / I->height );
        float Tx = ( W - I->width * S ) * 0.5f, Ty = ( H - I->height * S ) * 0.5f;
        Out.assign( size_t( W ) * H, 0 );
        nsvgRasterize( R, I, Tx, Ty, S, (unsigned char*)Out.data(), W, H, W * 4 );
        nsvgDeleteRasterizer( R );
        nsvgDelete( I );
        return Out;
    }

    TextureId RasterizeSvg( Renderer& R, const std::string& Svg, int W, int H )
    {
        auto Px = RasterizeSvgPixels( Svg, W, H );
        return Px.empty() ? 0 : R.CreateTexture( W, H, Px.data() );
    }

    SvgView::SvgView( std::string S, Vec2 Sz ) : Source( std::move( S ) ), Size( Sz )
    {
        if ( Size.x <= 0 || Size.y <= 0 )
        {
            float W = 24, H = 24;
            SvgSize( Source, W, H );
            if ( Size.x <= 0 && Size.y > 0 ) Size.x = Size.y * W / H;
            else if ( Size.y <= 0 && Size.x > 0 ) Size.y = Size.x * H / W;
            else Size = { W, H };
        }
    }

    SvgView::~SvgView()
    {
        if ( Tex )
            if ( Context* C = GetContext() ) C->GetRenderer().DestroyTexture( Tex );
    }

    void SvgView::SetSvg( std::string S )
    {
        Source = std::move( S );
        TexW = TexH = 0; // re-rasterize
        Redraw();
    }

    Vec2 SvgView::Measure( float ) { return Size; }

    void SvgView::Draw( DrawList& D )
    {
        Context* C = GetContext();
        if ( !C ) return;
        int W = std::max( 1, int( std::round( Bounds.w * D.Scale ) ) ), H = std::max( 1, int( std::round( Bounds.h * D.Scale ) ) );
        if ( W != TexW || H != TexH )
        {
            auto Px = RasterizeSvgPixels( Source, W, H );
            if ( Px.empty() ) return;
            if ( Tinted ) // keep only the coverage: drawn white, colored by Tint
                for ( auto& P : Px ) P = ( P & 0xFF000000u ) | 0x00FFFFFFu;
            if ( Tex ) C->GetRenderer().UpdateTexture( Tex, W, H, Px.data() );
            else Tex = C->GetRenderer().CreateTexture( W, H, Px.data() );
            TexW = W;
            TexH = H;
        }
        if ( Tex ) D.Image( Tex, Bounds, { 0, 0 }, { 1, 1 }, Tinted ? Tint.Resolve() : Color( 255, 255, 255 ) );
    }
}
