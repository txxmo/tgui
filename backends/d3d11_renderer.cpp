#include "d3d11_renderer.h"
#include "tg/draw.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace tg
{
    struct D3D11Renderer::Tex
    {
        ID3D11Texture2D* T = nullptr;
        ID3D11ShaderResourceView* Srv = nullptr;
        int W = 0, H = 0;
        void Release()
        {
            if ( Srv ) Srv->Release();
            if ( T ) T->Release();
            Srv = nullptr;
            T = nullptr;
        }
    };

    template<typename T> static void SafeRelease( T*& P ) { if ( P ) { P->Release(); P = nullptr; } }

    static const char* Shader = R"(
cbuffer Proj : register(b0) { float4x4 P; };
struct VsIn  { float2 pos : POSITION; float2 uv : TEXCOORD0; float4 col : COLOR0; };
struct PsIn  { float4 pos : SV_POSITION; float4 col : COLOR0; float2 uv : TEXCOORD0; };
PsIn vs( VsIn i ) { PsIn o; o.pos = mul( P, float4( i.pos, 0, 1 ) ); o.col = i.col; o.uv = i.uv; return o; }
Texture2D Tex : register(t0);
SamplerState Smp : register(s0);
float4 ps( PsIn i ) : SV_Target { return i.col * Tex.Sample( Smp, i.uv ); }
)";

    bool D3D11Renderer::Init( ID3D11Device* D, ID3D11DeviceContext* C )
    {
        Dev = D;
        Dc = C;
        Dev->AddRef();
        Dc->AddRef();

        // Shaders are compiled at startup with d3dcompiler_47 (ships with Windows 10+).
        ID3DBlob *VsBlob = nullptr, *PsBlob = nullptr, *Err = nullptr;
        if ( FAILED( D3DCompile( Shader, strlen( Shader ), "tg", nullptr, nullptr, "vs", "vs_4_0", 0, 0, &VsBlob, &Err ) ) ||
             FAILED( D3DCompile( Shader, strlen( Shader ), "tg", nullptr, nullptr, "ps", "ps_4_0", 0, 0, &PsBlob, &Err ) ) )
        {
            if ( Err ) { std::fprintf( stderr, "tg d3d11: %s\n", (const char*)Err->GetBufferPointer() ); Err->Release(); }
            SafeRelease( VsBlob );
            return false;
        }
        Dev->CreateVertexShader( VsBlob->GetBufferPointer(), VsBlob->GetBufferSize(), nullptr, &Vs );
        Dev->CreatePixelShader( PsBlob->GetBufferPointer(), PsBlob->GetBufferSize(), nullptr, &Ps );

        D3D11_INPUT_ELEMENT_DESC Elems[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, (UINT)offsetof( Vertex, Pos ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, (UINT)offsetof( Vertex, UV ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (UINT)offsetof( Vertex, Col ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        Dev->CreateInputLayout( Elems, 3, VsBlob->GetBufferPointer(), VsBlob->GetBufferSize(), &Layout );
        VsBlob->Release();
        PsBlob->Release();

        D3D11_BUFFER_DESC Cbd = {};
        Cbd.ByteWidth = 64;
        Cbd.Usage = D3D11_USAGE_DYNAMIC;
        Cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        Cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        Dev->CreateBuffer( &Cbd, nullptr, &Cb );

        D3D11_SAMPLER_DESC Sd = {};
        Sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        Sd.AddressU = Sd.AddressV = Sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        Sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        Dev->CreateSamplerState( &Sd, &Sampler );

        D3D11_BLEND_DESC Bd = {};
        Bd.RenderTarget[ 0 ].BlendEnable = TRUE;
        Bd.RenderTarget[ 0 ].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        Bd.RenderTarget[ 0 ].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        Bd.RenderTarget[ 0 ].BlendOp = D3D11_BLEND_OP_ADD;
        Bd.RenderTarget[ 0 ].SrcBlendAlpha = D3D11_BLEND_ONE;
        Bd.RenderTarget[ 0 ].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        Bd.RenderTarget[ 0 ].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        Bd.RenderTarget[ 0 ].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        Dev->CreateBlendState( &Bd, &Blend );

        D3D11_RASTERIZER_DESC Rd = {};
        Rd.FillMode = D3D11_FILL_SOLID;
        Rd.CullMode = D3D11_CULL_NONE;
        Rd.ScissorEnable = TRUE;
        Rd.DepthClipEnable = TRUE;
        Dev->CreateRasterizerState( &Rd, &Raster );

        D3D11_DEPTH_STENCIL_DESC Dd = {};
        Dd.DepthEnable = FALSE;
        Dd.StencilEnable = FALSE;
        Dev->CreateDepthStencilState( &Dd, &Depth );

        return Vs && Ps && Layout && Cb && Sampler && Blend && Raster && Depth;
    }

    void D3D11Renderer::Shutdown()
    {
        for ( Tex* T : Textures ) { T->Release(); delete T; }
        Textures.clear();
        SafeRelease( Vb ); SafeRelease( Ib ); SafeRelease( Cb );
        SafeRelease( Vs ); SafeRelease( Ps ); SafeRelease( Layout );
        SafeRelease( Sampler ); SafeRelease( Blend ); SafeRelease( Raster ); SafeRelease( Depth );
        SafeRelease( Dc ); SafeRelease( Dev );
        VbCap = IbCap = 0;
    }

    // ------------------------------------------------------------ textures (TextureId = Tex*, stable across resizes)

    bool D3D11Renderer::Alloc( Tex& T, int W, int H, const void* Pixels )
    {
        T.Release();
        D3D11_TEXTURE2D_DESC Td = {};
        Td.Width = UINT( W );
        Td.Height = UINT( H );
        Td.MipLevels = 1;
        Td.ArraySize = 1;
        Td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        Td.SampleDesc.Count = 1;
        Td.Usage = D3D11_USAGE_DEFAULT;
        Td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA Init = { Pixels, UINT( W * 4 ), 0 };
        if ( FAILED( Dev->CreateTexture2D( &Td, Pixels ? &Init : nullptr, &T.T ) ) ) return false;
        Dev->CreateShaderResourceView( T.T, nullptr, &T.Srv );
        T.W = W;
        T.H = H;
        return T.Srv != nullptr;
    }

    TextureId D3D11Renderer::CreateTexture( int W, int H, const void* Pixels )
    {
        Tex* T = new Tex();
        Alloc( *T, W, H, Pixels );
        Textures.push_back( T );
        return TextureId( T );
    }

    void D3D11Renderer::UpdateTexture( TextureId Id, int W, int H, const void* Pixels )
    {
        Tex* T = (Tex*)Id;
        if ( !T ) return;
        if ( T->W != W || T->H != H || !T->T ) { Alloc( *T, W, H, Pixels ); return; }
        if ( Pixels ) Dc->UpdateSubresource( T->T, 0, nullptr, Pixels, UINT( W * 4 ), 0 );
    }

    void D3D11Renderer::DestroyTexture( TextureId Id )
    {
        Tex* T = (Tex*)Id;
        auto It = std::find( Textures.begin(), Textures.end(), T );
        if ( It == Textures.end() ) return;
        T->Release();
        delete T;
        Textures.erase( It );
    }

    // ------------------------------------------------------------ render

    namespace
    {
        // Everything Render() changes, so an overlay leaves the host app's pipeline untouched.
        struct Backup
        {
            UINT NumScissor = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE, NumViewport = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
            D3D11_RECT Scissor[ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE ];
            D3D11_VIEWPORT Viewport[ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE ];
            ID3D11RasterizerState* Rs = nullptr;
            ID3D11BlendState* Bs = nullptr;
            FLOAT BlendFactor[ 4 ];
            UINT SampleMask = 0, StencilRef = 0;
            ID3D11DepthStencilState* Ds = nullptr;
            ID3D11ShaderResourceView* Srv = nullptr;
            ID3D11SamplerState* Smp = nullptr;
            ID3D11PixelShader* Ps = nullptr;
            ID3D11VertexShader* Vs = nullptr;
            ID3D11GeometryShader* Gs = nullptr;
            UINT PsInst = 256, VsInst = 256, GsInst = 256;
            ID3D11ClassInstance *PsI[ 256 ], *VsI[ 256 ], *GsI[ 256 ];
            D3D11_PRIMITIVE_TOPOLOGY Topo;
            ID3D11Buffer *Ib = nullptr, *Vb = nullptr, *VsCb = nullptr;
            UINT IbOffset = 0, VbStride = 0, VbOffset = 0;
            DXGI_FORMAT IbFormat;
            ID3D11InputLayout* Il = nullptr;

            void Save( ID3D11DeviceContext* C )
            {
                C->RSGetScissorRects( &NumScissor, Scissor );
                C->RSGetViewports( &NumViewport, Viewport );
                C->RSGetState( &Rs );
                C->OMGetBlendState( &Bs, BlendFactor, &SampleMask );
                C->OMGetDepthStencilState( &Ds, &StencilRef );
                C->PSGetShaderResources( 0, 1, &Srv );
                C->PSGetSamplers( 0, 1, &Smp );
                C->PSGetShader( &Ps, PsI, &PsInst );
                C->VSGetShader( &Vs, VsI, &VsInst );
                C->VSGetConstantBuffers( 0, 1, &VsCb );
                C->GSGetShader( &Gs, GsI, &GsInst );
                C->IAGetPrimitiveTopology( &Topo );
                C->IAGetIndexBuffer( &Ib, &IbFormat, &IbOffset );
                C->IAGetVertexBuffers( 0, 1, &Vb, &VbStride, &VbOffset );
                C->IAGetInputLayout( &Il );
            }

            void Restore( ID3D11DeviceContext* C )
            {
                C->RSSetScissorRects( NumScissor, Scissor );
                C->RSSetViewports( NumViewport, Viewport );
                C->RSSetState( Rs ); SafeRelease( Rs );
                C->OMSetBlendState( Bs, BlendFactor, SampleMask ); SafeRelease( Bs );
                C->OMSetDepthStencilState( Ds, StencilRef ); SafeRelease( Ds );
                C->PSSetShaderResources( 0, 1, &Srv ); SafeRelease( Srv );
                C->PSSetSamplers( 0, 1, &Smp ); SafeRelease( Smp );
                C->PSSetShader( Ps, PsI, PsInst ); SafeRelease( Ps );
                for ( UINT i = 0; i < PsInst; i++ ) SafeRelease( PsI[ i ] );
                C->VSSetShader( Vs, VsI, VsInst ); SafeRelease( Vs );
                C->VSSetConstantBuffers( 0, 1, &VsCb ); SafeRelease( VsCb );
                C->GSSetShader( Gs, GsI, GsInst ); SafeRelease( Gs );
                for ( UINT i = 0; i < VsInst; i++ ) SafeRelease( VsI[ i ] );
                for ( UINT i = 0; i < GsInst; i++ ) SafeRelease( GsI[ i ] );
                C->IASetPrimitiveTopology( Topo );
                C->IASetIndexBuffer( Ib, IbFormat, IbOffset ); SafeRelease( Ib );
                C->IASetVertexBuffers( 0, 1, &Vb, &VbStride, &VbOffset ); SafeRelease( Vb );
                C->IASetInputLayout( Il ); SafeRelease( Il );
            }
        };
    }

    void D3D11Renderer::Render( const DrawList& L, Vec2 Fb, float Scale )
    {
        if ( !Dev || Fb.x <= 0 || Fb.y <= 0 || L.Idx.empty() ) return;

        // grow buffers as needed
        if ( !Vb || VbCap < L.Vtx.size() )
        {
            SafeRelease( Vb );
            VbCap = L.Vtx.size() + 5000;
            D3D11_BUFFER_DESC D = {};
            D.ByteWidth = UINT( VbCap * sizeof( Vertex ) );
            D.Usage = D3D11_USAGE_DYNAMIC;
            D.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            D.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            if ( FAILED( Dev->CreateBuffer( &D, nullptr, &Vb ) ) ) return;
        }
        if ( !Ib || IbCap < L.Idx.size() )
        {
            SafeRelease( Ib );
            IbCap = L.Idx.size() + 10000;
            D3D11_BUFFER_DESC D = {};
            D.ByteWidth = UINT( IbCap * sizeof( uint32_t ) );
            D.Usage = D3D11_USAGE_DYNAMIC;
            D.BindFlags = D3D11_BIND_INDEX_BUFFER;
            D.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            if ( FAILED( Dev->CreateBuffer( &D, nullptr, &Ib ) ) ) return;
        }

        D3D11_MAPPED_SUBRESOURCE M;
        if ( FAILED( Dc->Map( Vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &M ) ) ) return;
        std::memcpy( M.pData, L.Vtx.data(), L.Vtx.size() * sizeof( Vertex ) );
        Dc->Unmap( Vb, 0 );
        if ( FAILED( Dc->Map( Ib, 0, D3D11_MAP_WRITE_DISCARD, 0, &M ) ) ) return;
        std::memcpy( M.pData, L.Idx.data(), L.Idx.size() * sizeof( uint32_t ) );
        Dc->Unmap( Ib, 0 );

        // orthographic projection in UI units, (0,0) top-left
        float W = Fb.x / Scale, H = Fb.y / Scale;
        const float Proj[ 4 ][ 4 ] = { { 2.f / W, 0, 0, 0 }, { 0, -2.f / H, 0, 0 }, { 0, 0, 0.5f, 0 }, { -1, 1, 0.5f, 1 } };
        if ( FAILED( Dc->Map( Cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &M ) ) ) return;
        std::memcpy( M.pData, Proj, sizeof( Proj ) );
        Dc->Unmap( Cb, 0 );

        Backup B;
        B.Save( Dc );

        D3D11_VIEWPORT Vp = { 0, 0, Fb.x, Fb.y, 0, 1 };
        Dc->RSSetViewports( 1, &Vp );
        UINT Stride = sizeof( Vertex ), Offset = 0;
        Dc->IASetInputLayout( Layout );
        Dc->IASetVertexBuffers( 0, 1, &Vb, &Stride, &Offset );
        Dc->IASetIndexBuffer( Ib, DXGI_FORMAT_R32_UINT, 0 );
        Dc->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
        Dc->VSSetShader( Vs, nullptr, 0 );
        Dc->VSSetConstantBuffers( 0, 1, &Cb );
        Dc->PSSetShader( Ps, nullptr, 0 );
        Dc->PSSetSamplers( 0, 1, &Sampler );
        Dc->GSSetShader( nullptr, nullptr, 0 );
        const float Factor[ 4 ] = { 0, 0, 0, 0 };
        Dc->OMSetBlendState( Blend, Factor, 0xFFFFFFFF );
        Dc->OMSetDepthStencilState( Depth, 0 );
        Dc->RSSetState( Raster );

        for ( const DrawCmd& C : L.Cmds )
        {
            if ( !C.IdxCount ) continue;
            D3D11_RECT R = { LONG( C.Clip.x * Scale ), LONG( C.Clip.y * Scale ), LONG( C.Clip.Right() * Scale + 0.5f ), LONG( C.Clip.Bottom() * Scale + 0.5f ) };
            if ( R.right <= R.left || R.bottom <= R.top ) continue;
            Dc->RSSetScissorRects( 1, &R );
            Tex* T = (Tex*)C.Texture;
            ID3D11ShaderResourceView* Srv = T ? T->Srv : nullptr;
            Dc->PSSetShaderResources( 0, 1, &Srv );
            Dc->DrawIndexed( C.IdxCount, C.IdxOffset, 0 );
        }

        if ( CaptureNext )
        {
            // copy the bound render target to a CPU-readable texture
            CaptureNext = false;
            ID3D11RenderTargetView* Rtv = nullptr;
            Dc->OMGetRenderTargets( 1, &Rtv, nullptr );
            ID3D11Resource* Res = nullptr;
            if ( Rtv ) { Rtv->GetResource( &Res ); Rtv->Release(); }
            ID3D11Texture2D* Src = nullptr;
            if ( Res ) { Res->QueryInterface( __uuidof( ID3D11Texture2D ), (void**)&Src ); Res->Release(); }
            if ( Src )
            {
                D3D11_TEXTURE2D_DESC Dd;
                Src->GetDesc( &Dd );
                Dd.Usage = D3D11_USAGE_STAGING;
                Dd.BindFlags = 0;
                Dd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                Dd.MiscFlags = 0;
                ID3D11Texture2D* Stage = nullptr;
                if ( Dd.SampleDesc.Count == 1 && SUCCEEDED( Dev->CreateTexture2D( &Dd, nullptr, &Stage ) ) )
                {
                    Dc->CopyResource( Stage, Src );
                    D3D11_MAPPED_SUBRESOURCE M;
                    if ( SUCCEEDED( Dc->Map( Stage, 0, D3D11_MAP_READ, 0, &M ) ) )
                    {
                        bool Bgra = Dd.Format == DXGI_FORMAT_B8G8R8A8_UNORM || Dd.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
                        Captured.W = int( Dd.Width );
                        Captured.H = int( Dd.Height );
                        Captured.Px.resize( size_t( Dd.Width ) * Dd.Height );
                        for ( UINT y = 0; y < Dd.Height; y++ )
                        {
                            const uint32_t* Row = (const uint32_t*)( (const uint8_t*)M.pData + size_t( y ) * M.RowPitch );
                            for ( UINT x = 0; x < Dd.Width; x++ )
                            {
                                uint32_t P = Row[ x ];
                                if ( Bgra ) P = ( P & 0xFF00FF00u ) | ( ( P & 0xFF ) << 16 ) | ( ( P >> 16 ) & 0xFF );
                                Captured.Px[ size_t( y ) * Dd.Width + x ] = P;
                            }
                        }
                        Dc->Unmap( Stage, 0 );
                    }
                    Stage->Release();
                }
                Src->Release();
            }
        }

        B.Restore( Dc );
    }
}
