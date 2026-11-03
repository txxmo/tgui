// Image — shows a texture. For screen shares / video: create the texture once with the renderer,
// then call Renderer::UpdateTexture( Tex, w, h, pixels ) each frame and Image::SetSource(...) on resize.
#pragma once
#include "../theme.h"
#include "../widget.h"

namespace tg
{
    enum class ImageFit : uint8_t { Stretch, Contain, Cover };

    struct ImageStyle
    {
        ImageFit Mode = ImageFit::Contain;
        float Radius = 0;
        Paint Letterbox = Color( 0, 0, 0, 255 ); // bars for Contain
        Color Tint = { 255, 255, 255 };
    };

    class Image : public Widget
    {
    public:
        Image() = default;
        Image( TextureId Tex, int Width, int Height ) { SetSource( Tex, Width, Height ); }

        ImageStyle Style;
        void SetSource( TextureId Tex, int Width, int Height );
        TextureId GetTexture() const { return Tex; }

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;

    private:
        TextureId Tex = 0;
        int TexW = 0, TexH = 0;
    };
}
