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
        // shaders run their Cpu function per pixel; backdrop blur and render targets work too
        ShaderId CreateShader( const ShaderDesc& Desc ) override;
        void DestroyShader( ShaderId S ) override;
        bool CanShade() const override { return true; }
        bool CanBackdrop() const override { return true; }
        TextureId CreateRenderTarget( int Width, int Height ) override;
        void RenderTo( TextureId Target, const DrawList& List, Vec2 Size, float Scale ) override;

        int Width = 0, Height = 0;
        std::vector<uint32_t> Pixels; // RGBA8 (Color::Pack layout)

        bool SavePNG( const std::string& Path ) const;

    private:
        struct Tex { int W = 0, H = 0; std::vector<uint32_t> Px; };
        std::unordered_map<TextureId, Tex> Textures;
        TextureId NextId = 1;
        std::unordered_map<ShaderId, ShaderDesc> Shaders;
        ShaderId NextShader = 1;
        float Time = 0;
        void Raster( const DrawList& List, float Scale, std::vector<uint32_t>& Dst, int W, int H );
    };
}
