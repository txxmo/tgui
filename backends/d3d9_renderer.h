// Direct3D 9 renderer — for DX9 apps and overlays (what the old tgui drew with).
// Saves/restores device state with a state block, so it's safe inside a host's EndScene.
//
//   tg::D3D9Renderer Dx;  Dx.Init( Device );
//   per frame, between BeginScene/EndScene:  Ctx.Frame(); Ctx.Render( { BackBufferW, BackBufferH } );
//   around IDirect3DDevice9::Reset:  Dx.InvalidateDeviceObjects(); Reset(...); Dx.CreateDeviceObjects();
#pragma once
#include "tg/renderer.h"
#include <vector>

struct IDirect3DDevice9;
struct IDirect3DVertexBuffer9;
struct IDirect3DIndexBuffer9;
struct IDirect3DTexture9;

namespace tg
{
    class D3D9Renderer : public Renderer
    {
    public:
        ~D3D9Renderer() override { Shutdown(); }

        bool Init( IDirect3DDevice9* Device );
        void Shutdown();
        void InvalidateDeviceObjects(); // before IDirect3DDevice9::Reset
        bool CreateDeviceObjects();     // after Reset

        TextureId CreateTexture( int W, int H, const void* Pixels ) override;
        void UpdateTexture( TextureId Tex, int W, int H, const void* Pixels ) override;
        void DestroyTexture( TextureId Tex ) override;
        void Render( const DrawList& List, Vec2 FramebufferSize, float Scale = 1.f ) override;

    private:
        struct Tex;
        IDirect3DDevice9* Dev = nullptr;
        IDirect3DVertexBuffer9* Vb = nullptr;
        IDirect3DIndexBuffer9* Ib = nullptr;
        size_t VbCap = 0, IbCap = 0;
        std::vector<Tex*> Textures;
        bool Upload( Tex& T, int W, int H, const void* Pixels );
    };
}
