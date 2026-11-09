// Direct3D 11 renderer. Works in your own app (tg::App uses it on Windows) or as an overlay inside
// someone else's D3D11 render loop: it saves and restores the device state it touches.
//
//   tg::D3D11Renderer Dx;
//   Dx.Init( Device, DeviceContext );
//   ... each frame, with your back buffer bound as render target:
//   Ctx.Frame(); Ctx.Render( { BackBufferWidth, BackBufferHeight } );
#pragma once
#include "tg/renderer.h"
#include <vector>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Buffer;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;
struct ID3D11SamplerState;
struct ID3D11BlendState;
struct ID3D11RasterizerState;
struct ID3D11DepthStencilState;

namespace tg
{
    class D3D11Renderer : public Renderer
    {
    public:
        ~D3D11Renderer() override { Shutdown(); }

        bool Init( ID3D11Device* Device, ID3D11DeviceContext* DeviceContext );
        void Shutdown();

        TextureId CreateTexture( int W, int H, const void* Pixels ) override;
        void UpdateTexture( TextureId Tex, int W, int H, const void* Pixels ) override;
        void DestroyTexture( TextureId Tex ) override;
        bool CanCapture() const override { return true; }
        void Render( const DrawList& List, Vec2 FramebufferSize, float Scale = 1.f ) override;
        ShaderId CreateShader( const ShaderDesc& Desc ) override; // ShaderDesc::Hlsl
        void DestroyShader( ShaderId S ) override;
        bool CanShade() const override { return true; }
        bool CanBackdrop() const override { return true; }
        TextureId CreateRenderTarget( int Width, int Height ) override;
        void RenderTo( TextureId Target, const DrawList& List, Vec2 Size, float Scale ) override;

    private:
        struct Tex;
        ID3D11Device* Dev = nullptr;
        ID3D11DeviceContext* Dc = nullptr;
        ID3D11Buffer *Vb = nullptr, *Ib = nullptr, *Cb = nullptr;
        size_t VbCap = 0, IbCap = 0;
        ID3D11VertexShader* Vs = nullptr;
        ID3D11PixelShader* Ps = nullptr;
        ID3D11InputLayout* Layout = nullptr;
        ID3D11SamplerState* Sampler = nullptr;
        ID3D11BlendState* Blend = nullptr;
        ID3D11RasterizerState* Raster = nullptr;
        ID3D11DepthStencilState* Depth = nullptr;
        std::vector<Tex*> Textures;
        bool Alloc( Tex& T, int W, int H, const void* Pixels, bool Target = false, int Format = 0 );
        ID3D11Buffer* ParamCb = nullptr;
        ID3D11PixelShader *BlurPs = nullptr, *MaskPs = nullptr;
        ID3D11BlendState* Premul = nullptr;
        ID3D11Buffer *QVb = nullptr, *QIb = nullptr;
        std::vector<std::pair<ShaderId, ID3D11PixelShader*>> Custom;
        ShaderId NextShader = 1;
        Tex *Back = nullptr, *Ping[ 2 ] = {};
        void Draw( const DrawList& L, Vec2 Fb, float Scale );
        void DoBackdrop( const struct DrawCmd& C, Vec2 Fb, float Scale );
        void SetProj( Vec2 Fb, float Scale, float Time );
        void Quad( Rect R, Vec2 Uv0, Vec2 Uv1 );
    };
}
