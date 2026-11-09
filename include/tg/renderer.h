// tg/renderer.h — the only thing a graphics backend has to implement.
//
// Backends live in /backends (OpenGL 3, software). Writing a D3D11/Vulkan/Metal one
// means implementing these five functions; nothing else in tg touches the GPU.
#pragma once
#include "core.h"
#include <string>
#include <functional>
#include <vector>

namespace tg
{
    class DrawList;

    // A pixel shader in up to three dialects; a renderer uses the one it understands.
    //   Glsl (GL3) and Hlsl (D3D11) define    vec4 / float4 effect( color, uv, pos )
    //     color: vertex color · uv: texture coords · pos: pixel position (top-left origin)
    //     available: Texture (sample with tex( uv )), Params[ 8 ] (vec4), Time, Resolution (pixels)
    //   Cpu: the same in C++, for the software renderer (tests, screenshots).
    struct ShaderDesc
    {
        std::string Glsl;
        std::string Hlsl;
        std::function<Color( Color In, Vec2 UV, Vec2 Pos, const float* Params, const std::function<Color( Vec2 )>& Tex )> Cpu;
    };

    class Renderer
    {
    public:
        virtual ~Renderer() = default;

        // Pixels are tightly packed RGBA8 (Width*Height*4 bytes).
        virtual TextureId CreateTexture( int Width, int Height, const void* Pixels ) = 0;
        // Replace the whole texture contents (e.g. a new screen-share / video frame).
        virtual void UpdateTexture( TextureId Tex, int Width, int Height, const void* Pixels ) = 0;
        virtual void DestroyTexture( TextureId Tex ) = 0;

        // Draw a frame. FramebufferSize is in pixels; the DrawList is in UI units (Scale converts).
        virtual void Render( const DrawList& List, Vec2 FramebufferSize, float Scale = 1.f ) = 0;

        // ---- frame capture (eyedropper, screenshots). Set CaptureNext before Render(); afterwards Captured
        //      holds the frame (RGBA in Color::Pack layout, top row first, framebuffer pixels).
        bool CaptureNext = false;
        struct Capture
        {
            int W = 0, H = 0;
            std::vector<uint32_t> Px;
            bool Valid() const { return W > 0 && H > 0 && Px.size() == size_t( W ) * H; }
            uint32_t At( int X, int Y ) const { return X >= 0 && Y >= 0 && X < W && Y < H ? Px[ size_t( Y ) * W + X ] : 0; }
        } Captured;
        virtual bool CanCapture() const { return false; }

        // ---- custom shaders (see ShaderDesc). 0 = not supported here: the draw falls back to normal.
        virtual ShaderId CreateShader( const struct ShaderDesc& Desc ) { (void)Desc; return 0; }
        virtual void DestroyShader( ShaderId S ) { (void)S; }
        virtual bool CanShade() const { return false; }
        virtual bool CanBackdrop() const { return false; } // DrawList::BackdropBlur

        // ---- render to texture: an offscreen target you can draw into (Render with Target) and then
        //      draw like any texture. Rows are top-down like every other texture.
        virtual TextureId CreateRenderTarget( int Width, int Height ) { (void)Width; (void)Height; return 0; }
        virtual void RenderTo( TextureId Target, const DrawList& List, Vec2 Size, float Scale ) { (void)Target; (void)List; (void)Size; (void)Scale; }
    };
}
