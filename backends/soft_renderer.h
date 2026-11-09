// Software renderer: rasterizes a DrawList into an RGBA buffer on the CPU.
// No GPU needed — used for headless screenshots, tests, and as a reference for writing other backends.
#pragma once
#include "tg/renderer.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace tg
{
    class SoftRenderer : public Renderer
    {
    public:
        TextureId CreateTexture( int W, int H, const void* Pixels ) override;
        void UpdateTexture( TextureId Tex, int W, int H, const void* Pixels ) override;
        void DestroyTexture( TextureId Tex ) override;
        bool CanCapture() const override { return true; }
        void Render( const DrawList& List, Vec2 FramebufferSize, float Scale = 1.f ) override;

        int Width = 0, Height = 0;
        std::vector<uint32_t> Pixels; // RGBA8 (Color::Pack layout)

        bool SavePNG( const std::string& Path ) const;

    private:
        struct Tex { int W = 0, H = 0; std::vector<uint32_t> Px; };
        std::unordered_map<TextureId, Tex> Textures;
        TextureId NextId = 1;
    };
}
