// SVG — vector images and icons, rasterized at the exact pixel size (crisp at any DPI).
//
//   W.Svg( LogoSvgText, { 120, 40 } );                         // as is
//   auto& I = W.Svg( LoadFile( "icons/cpu.svg" ), { 24, 24 } );
//   I.Tint = tg::Tok::Accent;                                   // one-color icons: draw in this color
//   tg::TextureId T = tg::RasterizeSvg( Renderer, Text, 256, 256 ); // or just a texture
#pragma once
#include "renderer.h"
#include "theme.h"
#include "widget.h"
#include <memory>
#include <string>
#include <vector>

namespace tg
{
    // RGBA pixels (Color::Pack layout) of an SVG at W x H (aspect kept, centered). Empty on parse error.
    std::vector<uint32_t> RasterizeSvgPixels( const std::string& Svg, int W, int H );
    TextureId RasterizeSvg( Renderer& R, const std::string& Svg, int W, int H );
    bool SvgSize( const std::string& Svg, float& W, float& H ); // the document's own size

    class SvgView : public Widget
    {
    public:
        SvgView( std::string Svg, Vec2 Size = { 0, 0 } ); // Size 0 = the document's size
        ~SvgView() override;
        void SetSvg( std::string Svg );
        bool Tinted = false;      // treat the image as a shape and fill it with Tint
        Paint Tint = Tok::Text;
        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;

    private:
        std::string Source;
        Vec2 Size;
        TextureId Tex = 0;
        int TexW = 0, TexH = 0;
    };
}
