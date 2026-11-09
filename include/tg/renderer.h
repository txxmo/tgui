// tg/renderer.h — the only thing a graphics backend has to implement.
//
// Backends live in /backends (OpenGL 3, software). Writing a D3D11/Vulkan/Metal one
// means implementing these five functions; nothing else in tg touches the GPU.
#pragma once
#include "core.h"
#include <vector>

namespace tg
{
    class DrawList;

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
    };
}
