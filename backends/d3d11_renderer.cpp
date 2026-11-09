#include "d3d11_renderer.h"
#include "tg/draw.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace tg
{
    struct D3D11Renderer::Tex
    {
        ID3D11Texture2D* T = nullptr;
        ID3D11ShaderResourceView* Srv = nullptr;
        ID3D11RenderTargetView* Rtv = nullptr; // render targets only
        int W = 0, H = 0, Format = 0;
        void Release()
        {
            if ( Rtv ) Rtv->Release();
            Rtv = nullptr;
            if ( Srv ) Srv->Release();
            if ( T ) T->Release();
            Srv = nullptr;
            T = nullptr;
        }
    };

    template<typename T> static void SafeRelease( T*& P ) { if ( P ) { P->Release(); P = nullptr; } }

    // Info = ( Scale, Time, framebuffer width, height )
    static const char* Common = R"(
cbuffer Proj : register(b0) { float4x4 P; float4 Info; };
cbuffer Fx : register(b1) { float4 Params[8]; };
struct VsIn  { float2 pos : POSITION; float2 uv : TEXCOORD0; float4 col : COLOR0; };
struct PsIn  { float4 pos : SV_POSITION; float4 col : COLOR0; float2 uv : TEXCOORD0; float2 px : TEXCOORD1; };
Texture2D Tex : register(t0);
SamplerState Smp : register(s0);
#define Time Info.y
#define Resolution Info.zw
float4 tex( float2 uv ) { return Tex.Sample( Smp, uv ); }
)";

    static const char* Shader = R"(
PsIn vs( VsIn i ) { PsIn o; o.pos = mul( P, float4( i.pos, 0, 1 ) ); o.col = i.col; o.uv = i.uv; o.px = i.pos * Info.x; return o; }
float4 ps( PsIn i ) : SV_Target { return i.col * Tex.Sample( Smp, i.uv ); }
float4 blur( PsIn i ) : SV_Target
{
    float4 C = 0; float W = 0;
    [unroll] for ( int k = -8; k <= 8; k++ ) { float w = exp( -0.5 * k * k / 10.0 ); C += Tex.Sample( Smp, i.uv + Params[0].xy * k ) * w; W += w; }
    return C / W;
}
float4 mask( PsIn i ) : SV_Target
{
    float4 R = Params[0]; float Rad = Params[1].x;
    float2 H = R.zw * 0.5, Q = abs( i.px - ( R.xy + H ) ) - ( H - Rad );
    float D = length( max( Q, 0 ) ) + min( max( Q.x, Q.y ), 0 ) - Rad;
    float4 C = Tex.Sample( Smp, i.uv );
    return float4( C.rgb, 1 ) * saturate( 0.5 - D );
}
)";

    static ID3DBlob* CompileHlsl( const std::string& Src, const char* Entry, const char* Profile )
    {
        ID3DBlob *B = nullptr, *Err = nullptr;
        if ( FAILED( D3DCompile( Src.c_str(), Src.size(), "tg", nullptr, nullptr, Entry, Profile, 0, 0, &B, &Err ) ) )
        {
            if ( Err ) { std::fprintf( stderr, "tg d3d11: %s\n", (const char*)Err->GetBufferPointer() ); Err->Release(); }
            return nullptr;
        }
        if ( Err ) Err->Release();
        return B;
    }

    bool D3D11Renderer::Init( ID3D11Device* D, ID3D11DeviceContext* C )
    {
        Dev = D;
        Dc = C;
        Dev->AddRef();
        Dc->AddRef();

        // Shaders are compiled at startup with d3dcompiler_47 (ships with Windows 10+).
        std::string Src = std::string( Common ) + Shader;
        ID3DBlob* VsBlob = CompileHlsl( Src, "vs", "vs_4_0" );
        ID3DBlob* PsBlob = CompileHlsl( Src, "ps", "ps_4_0" );
        ID3DBlob* BlurBlob = CompileHlsl( Src, "blur", "ps_4_0" );
        ID3DBlob* MaskBlob = CompileHlsl( Src, "mask", "ps_4_0" );
        if ( !VsBlob || !PsBlob || !BlurBlob || !MaskBlob )
        {
            SafeRelease( VsBlob ); SafeRelease( PsBlob ); SafeRelease( BlurBlob ); SafeRelease( MaskBlob );
            return false;
        }
        Dev->CreateVertexShader( VsBlob->GetBufferPointer(), VsBlob->GetBufferSize(), nullptr, &Vs );
        Dev->CreatePixelShader( PsBlob->GetBufferPointer(), PsBlob->GetBufferSize(), nullptr, &Ps );
        Dev->CreatePixelShader( BlurBlob->GetBufferPointer(), BlurBlob->GetBufferSize(), nullptr, &BlurPs );
        Dev->CreatePixelShader( MaskBlob->GetBufferPointer(), MaskBlob->GetBufferSize(), nullptr, &MaskPs );
        BlurBlob->Release();
        MaskBlob->Release();

        D3D11_INPUT_ELEMENT_DESC Elems[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, (UINT)offsetof( Vertex, Pos ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, (UINT)offsetof( Vertex, UV ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (UINT)offsetof( Vertex, Col ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        Dev->CreateInputLayout( Elems, 3, VsBlob->GetBufferPointer(), VsBlob->GetBufferSize(), &Layout );
        VsBlob->Release();
        PsBlob->Release();

        D3D11_BUFFER_DESC Cbd = {};
        Cbd.ByteWidth = 128; // shader params (8 float4)
        Cbd.Usage = D3D11_USAGE_DYNAMIC;
        Cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        Cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        Dev->CreateBuffer( &Cbd, nullptr, &ParamCb );
        Cbd.ByteWidth = 80; // projection + info
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
        Bd.RenderTarget[ 0 ].SrcBlend = D3D11_BLEND_ONE; // premultiplied (blurred backdrop)
        Dev->CreateBlendState( &Bd, &Premul );

        // a dynamic 4-vertex quad for the backdrop passes
        D3D11_BUFFER_DESC Qd = {};
        Qd.ByteWidth = UINT( 4 * sizeof( Vertex ) );
        Qd.Usage = D3D11_USAGE_DYNAMIC;
        Qd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        Qd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        Dev->CreateBuffer( &Qd, nullptr, &QVb );
        const uint32_t QIdx[ 6 ] = { 0, 1, 2, 0, 2, 3 };
        D3D11_BUFFER_DESC Qi = {};
        Qi.ByteWidth = sizeof( QIdx );
        Qi.Usage = D3D11_USAGE_IMMUTABLE;
        Qi.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA QInit = { QIdx, 0, 0 };
        Dev->CreateBuffer( &Qi, &QInit, &QIb );

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

        return Vs && Ps && Layout && Cb && ParamCb && Sampler && Blend && Premul && Raster && Depth && BlurPs && MaskPs && QVb && QIb;
    }

    void D3D11Renderer::Shutdown()
    {
        for ( Tex* T : Textures ) { T->Release(); delete T; }
        Textures.clear();
        SafeRelease( Vb ); SafeRelease( Ib ); SafeRelease( Cb );
        SafeRelease( Vs ); SafeRelease( Ps ); SafeRelease( Layout );
        SafeRelease( Sampler ); SafeRelease( Blend ); SafeRelease( Raster ); SafeRelease( Depth );
        SafeRelease( ParamCb ); SafeRelease( BlurPs ); SafeRelease( MaskPs ); SafeRelease( Premul ); SafeRelease( QVb ); SafeRelease( QIb );
        for ( auto& [ Id, P ] : Custom ) P->Release();
        Custom.clear();
        for ( Tex** T : { &Back, &Ping[ 0 ], &Ping[ 1 ] } ) if ( *T ) { ( *T )->Release(); delete *T; *T = nullptr; }
        SafeRelease( Dc ); SafeRelease( Dev );
        VbCap = IbCap = 0;
    }

    // ------------------------------------------------------------ textures (TextureId = Tex*, stable across resizes)

    bool D3D11Renderer::Alloc( Tex& T, int W, int H, const void* Pixels, bool Target, int Format )
    {
        T.Release();
        D3D11_TEXTURE2D_DESC Td = {};
        Td.Width = UINT( std::max( 1, W ) );
        Td.Height = UINT( std::max( 1, H ) );
        Td.MipLevels = 1;
        Td.ArraySize = 1;
        Td.Format = Format ? DXGI_FORMAT( Format ) : DXGI_FORMAT_R8G8B8A8_UNORM;
        Td.SampleDesc.Count = 1;
        Td.Usage = D3D11_USAGE_DEFAULT;
        Td.BindFlags = D3D11_BIND_SHADER_RESOURCE | ( Target ? D3D11_BIND_RENDER_TARGET : 0 );
        D3D11_SUBRESOURCE_DATA Init = { Pixels, UINT( W * 4 ), 0 };
        if ( FAILED( Dev->CreateTexture2D( &Td, Pixels ? &Init : nullptr, &T.T ) ) ) return false;
        Dev->CreateShaderResourceView( T.T, nullptr, &T.Srv );
        if ( Target ) Dev->CreateRenderTargetView( T.T, nullptr, &T.Rtv );
        T.W = W;
        T.H = H;
        T.Format = int( Td.Format );
        return T.Srv != nullptr && ( !Target || T.Rtv );
    }

    TextureId D3D11Renderer::CreateRenderTarget( int W, int H )
    {
        Tex* T = new Tex();
        if ( !Alloc( *T, W, H, nullptr, true ) ) { T->Release(); delete T; return 0; }
        Textures.push_back( T );
        return TextureId( T );
    }

    ShaderId D3D11Renderer::CreateShader( const ShaderDesc& D )
    {
        if ( D.Hlsl.empty() ) return 0;
        std::string Src = std::string( Common ) + "#line 1\n" + D.Hlsl + "\nfloat4 main_ps( PsIn i ) : SV_Target { return effect( i.col, i.uv, i.px ); }\n";
        ID3DBlob* B = CompileHlsl( Src, "main_ps", "ps_4_0" );
        if ( !B ) return 0;
        ID3D11PixelShader* P = nullptr;
        Dev->CreatePixelShader( B->GetBufferPointer(), B->GetBufferSize(), nullptr, &P );
        B->Release();
        if ( !P ) return 0;
        Custom.push_back( { NextShader, P } );
        return NextShader++;
    }

    void D3D11Renderer::DestroyShader( ShaderId S )
    {
        for ( size_t i = 0; i < Custom.size(); i++ )
            if ( Custom[ i ].first == S ) { Custom[ i ].second->Release(); Custom.erase( Custom.begin() + long( i ) ); return; }
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

    void D3D11Renderer::SetProj( Vec2 Fb, float Scale, float Time )
    {
        // orthographic projection in UI units, (0,0) top-left; + Info
        float W = Fb.x / Scale, H = Fb.y / Scale;
        const float Data[ 20 ] = { 2.f / W, 0, 0, 0, 0, -2.f / H, 0, 0, 0, 0, 0.5f, 0, -1, 1, 0.5f, 1, Scale, Time, Fb.x, Fb.y };
        D3D11_MAPPED_SUBRESOURCE M;
        if ( FAILED( Dc->Map( Cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &M ) ) ) return;
        std::memcpy( M.pData, Data, sizeof( Data ) );
        Dc->Unmap( Cb, 0 );
    }

    void D3D11Renderer::Quad( Rect R, Vec2 Uv0, Vec2 Uv1 )
    {
        Vertex V[ 4 ] = { { R.Pos(), Uv0, 0xFFFFFFFFu }, { { R.Right(), R.y }, { Uv1.x, Uv0.y }, 0xFFFFFFFFu },
                          { R.Max(), Uv1, 0xFFFFFFFFu }, { { R.x, R.Bottom() }, { Uv0.x, Uv1.y }, 0xFFFFFFFFu } };
        D3D11_MAPPED_SUBRESOURCE M;
        if ( FAILED( Dc->Map( QVb, 0, D3D11_MAP_WRITE_DISCARD, 0, &M ) ) ) return;
        std::memcpy( M.pData, V, sizeof( V ) );
        Dc->Unmap( QVb, 0 );
        UINT Stride = sizeof( Vertex ), Offset = 0;
        Dc->IASetVertexBuffers( 0, 1, &QVb, &Stride, &Offset );
        Dc->IASetIndexBuffer( QIb, DXGI_FORMAT_R32_UINT, 0 );
        Dc->DrawIndexed( 6, 0, 0 );
    }

    void D3D11Renderer::DoBackdrop( const DrawCmd& C, Vec2 Fb, float Scale )
    {
        Rect A = C.Area.Intersect( C.Clip );
        if ( A.Empty() ) return;
        ID3D11RenderTargetView* Rtv = nullptr;
        Dc->OMGetRenderTargets( 1, &Rtv, nullptr );
        if ( !Rtv ) return;
        ID3D11Resource* Res = nullptr;
        Rtv->GetResource( &Res );
        ID3D11Texture2D* Src = nullptr;
        if ( Res ) { Res->QueryInterface( __uuidof( ID3D11Texture2D ), (void**)&Src ); Res->Release(); }
        if ( !Src ) { Rtv->Release(); return; }
        D3D11_TEXTURE2D_DESC Sd;
        Src->GetDesc( &Sd );
        if ( Sd.SampleDesc.Count != 1 ) { Src->Release(); Rtv->Release(); return; } // MSAA back buffers: skip
        float Pad = C.Amount * 1.5f;
        int X0 = std::max( 0, int( ( A.x - Pad ) * Scale ) ), Y0 = std::max( 0, int( ( A.y - Pad ) * Scale ) );
        int X1 = std::min( int( Fb.x ), int( std::ceil( ( A.Right() + Pad ) * Scale ) ) ), Y1 = std::min( int( Fb.y ), int( std::ceil( ( A.Bottom() + Pad ) * Scale ) ) );
        int W = X1 - X0, H = Y1 - Y0;
        if ( W <= 0 || H <= 0 ) { Src->Release(); Rtv->Release(); return; }
        // 1) copy the region behind (same format as the target)
        if ( !Back ) Back = new Tex();
        if ( Back->W < W || Back->H < H || Back->Format != int( Sd.Format ) ) Alloc( *Back, std::max( W, Back->W ), std::max( H, Back->H ), nullptr, false, int( Sd.Format ) );
        D3D11_BOX Box = { UINT( X0 ), UINT( Y0 ), 0, UINT( X1 ), UINT( Y1 ), 1 };
        Dc->CopySubresourceRegion( Back->T, 0, 0, 0, 0, Src, 0, &Box );
        Src->Release();
        // 2) blur at half size: Back -> Ping0 (horizontal) -> Ping1 (vertical)
        int Hw = std::max( 1, W / 2 ), Hh = std::max( 1, H / 2 );
        for ( Tex*& P : Ping )
        {
            if ( !P ) P = new Tex();
            if ( P->W < Hw || P->H < Hh ) Alloc( *P, std::max( Hw, P->W ), std::max( Hh, P->H ), nullptr, true );
        }
        const float Factor[ 4 ] = { 0, 0, 0, 0 };
        Dc->OMSetBlendState( nullptr, Factor, 0xFFFFFFFF );
        Dc->RSSetScissorRects( 0, nullptr );
        D3D11_RECT Full = { 0, 0, 16384, 16384 };
        Dc->RSSetScissorRects( 1, &Full );
        float Step = std::max( 1.f, C.Amount * Scale / 2 / 8 );
        ID3D11ShaderResourceView* Null = nullptr;
        auto Pass = [ & ]( Tex* From, Tex* To, float Ux, float Uy, Vec2 Uv1 ) {
            Dc->PSSetShaderResources( 0, 1, &Null );
            Dc->OMSetRenderTargets( 1, &To->Rtv, nullptr );
            D3D11_VIEWPORT Vp = { 0, 0, float( To->W ), float( To->H ), 0, 1 };
            Dc->RSSetViewports( 1, &Vp );
            SetProj( { float( To->W ), float( To->H ) }, 1, 0 );
            const float P[ 32 ] = { Ux, Uy };
            D3D11_MAPPED_SUBRESOURCE M;
            if ( SUCCEEDED( Dc->Map( ParamCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &M ) ) ) { std::memcpy( M.pData, P, sizeof( P ) ); Dc->Unmap( ParamCb, 0 ); }
            Dc->PSSetShader( BlurPs, nullptr, 0 );
            Dc->PSSetShaderResources( 0, 1, &From->Srv );
            Quad( { 0, 0, float( Hw ), float( Hh ) }, { 0, 0 }, Uv1 );
        };
        Pass( Back, Ping[ 0 ], Step * 2 / Back->W, 0, { float( W ) / Back->W, float( H ) / Back->H } );
        Pass( Ping[ 0 ], Ping[ 1 ], 0, Step / Ping[ 0 ]->H, { float( Hw ) / Ping[ 0 ]->W, float( Hh ) / Ping[ 0 ]->H } );
        // 3) back onto the target through the rounded mask (premultiplied)
        Dc->PSSetShaderResources( 0, 1, &Null );
        Dc->OMSetRenderTargets( 1, &Rtv, nullptr );
        Rtv->Release();
        D3D11_VIEWPORT Vp = { 0, 0, Fb.x, Fb.y, 0, 1 };
        Dc->RSSetViewports( 1, &Vp );
        SetProj( Fb, Scale, 0 );
        const float P[ 32 ] = { A.x * Scale, A.y * Scale, A.w * Scale, A.h * Scale, C.Radius * Scale };
        D3D11_MAPPED_SUBRESOURCE M;
        if ( SUCCEEDED( Dc->Map( ParamCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &M ) ) ) { std::memcpy( M.pData, P, sizeof( P ) ); Dc->Unmap( ParamCb, 0 ); }
        Dc->OMSetBlendState( Premul, Factor, 0xFFFFFFFF );
        Dc->PSSetShader( MaskPs, nullptr, 0 );
        Dc->PSSetShaderResources( 0, 1, &Ping[ 1 ]->Srv );
        float Gx = X0 / Scale, Gy = Y0 / Scale, Gw = W / Scale, Gh = H / Scale;
        float U = float( Hw ) / Ping[ 1 ]->W, V = float( Hh ) / Ping[ 1 ]->H;
        Quad( A, { ( A.x - Gx ) / Gw * U, ( A.y - Gy ) / Gh * V }, { ( A.Right() - Gx ) / Gw * U, ( A.Bottom() - Gy ) / Gh * V } );
        Dc->OMSetBlendState( Blend, Factor, 0xFFFFFFFF );
    }

    void D3D11Renderer::Draw( const DrawList& L, Vec2 Fb, float Scale )
    {
        if ( L.Idx.empty() && L.Cmds.empty() ) return;
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
        if ( !L.Vtx.empty() )
        {
            if ( FAILED( Dc->Map( Vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &M ) ) ) return;
            std::memcpy( M.pData, L.Vtx.data(), L.Vtx.size() * sizeof( Vertex ) );
            Dc->Unmap( Vb, 0 );
        }
        if ( !L.Idx.empty() )
        {
            if ( FAILED( Dc->Map( Ib, 0, D3D11_MAP_WRITE_DISCARD, 0, &M ) ) ) return;
            std::memcpy( M.pData, L.Idx.data(), L.Idx.size() * sizeof( uint32_t ) );
            Dc->Unmap( Ib, 0 );
        }
        auto Setup = [ & ] {
            SetProj( Fb, Scale, L.Time );
            D3D11_VIEWPORT Vp = { 0, 0, Fb.x, Fb.y, 0, 1 };
            Dc->RSSetViewports( 1, &Vp );
            UINT Stride = sizeof( Vertex ), Offset = 0;
            Dc->IASetInputLayout( Layout );
            Dc->IASetVertexBuffers( 0, 1, &Vb, &Stride, &Offset );
            Dc->IASetIndexBuffer( Ib, DXGI_FORMAT_R32_UINT, 0 );
            Dc->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
            Dc->VSSetShader( Vs, nullptr, 0 );
            Dc->VSSetConstantBuffers( 0, 1, &Cb );
            Dc->PSSetConstantBuffers( 0, 1, &Cb );
            Dc->PSSetConstantBuffers( 1, 1, &ParamCb );
            Dc->PSSetShader( Ps, nullptr, 0 );
            Dc->PSSetSamplers( 0, 1, &Sampler );
            Dc->GSSetShader( nullptr, nullptr, 0 );
            const float Factor[ 4 ] = { 0, 0, 0, 0 };
            Dc->OMSetBlendState( Blend, Factor, 0xFFFFFFFF );
            Dc->OMSetDepthStencilState( Depth, 0 );
            Dc->RSSetState( Raster );
        };
        Setup();
        ShaderId Bound = 0;
        uint32_t BoundParams = ~0u;
        for ( const DrawCmd& C : L.Cmds )
        {
            if ( C.Kind == DrawCmd::Backdrop )
            {
                DoBackdrop( C, Fb, Scale );
                Setup();
                Bound = 0;
                BoundParams = ~0u;
                continue;
            }
            if ( !C.IdxCount ) continue;
            D3D11_RECT R = { LONG( C.Clip.x * Scale ), LONG( C.Clip.y * Scale ), LONG( C.Clip.Right() * Scale + 0.5f ), LONG( C.Clip.Bottom() * Scale + 0.5f ) };
            if ( R.right <= R.left || R.bottom <= R.top ) continue;
            if ( C.Shader != Bound )
            {
                ID3D11PixelShader* P = Ps;
                for ( auto& [ Id, Cp ] : Custom )
                    if ( Id == C.Shader ) P = Cp;
                Dc->PSSetShader( P, nullptr, 0 );
                Bound = C.Shader;
            }
            if ( C.Shader && C.ParamOffset != BoundParams && C.ParamOffset + 32 <= L.Params.size() )
            {
                D3D11_MAPPED_SUBRESOURCE Pm;
                if ( SUCCEEDED( Dc->Map( ParamCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &Pm ) ) )
                {
                    std::memcpy( Pm.pData, L.Params.data() + C.ParamOffset, 32 * sizeof( float ) );
                    Dc->Unmap( ParamCb, 0 );
                }
                BoundParams = C.ParamOffset;
            }
            Dc->RSSetScissorRects( 1, &R );
            Tex* T = (Tex*)C.Texture;
            ID3D11ShaderResourceView* Srv = T ? T->Srv : nullptr;
            Dc->PSSetShaderResources( 0, 1, &Srv );
            Dc->DrawIndexed( C.IdxCount, C.IdxOffset, 0 );
        }
        ID3D11ShaderResourceView* Null = nullptr;
        Dc->PSSetShaderResources( 0, 1, &Null );
    }

    void D3D11Renderer::RenderTo( TextureId Id, const DrawList& L, Vec2 Size, float Scale )
    {
        Tex* T = (Tex*)Id;
        if ( !T || !Dev ) return;
        if ( T->W != int( Size.x ) || T->H != int( Size.y ) || !T->Rtv ) Alloc( *T, int( Size.x ), int( Size.y ), nullptr, true );
        if ( !T->Rtv ) return;
        Backup B;
        B.Save( Dc );
        ID3D11RenderTargetView* PrevRtv = nullptr;
        ID3D11DepthStencilView* PrevDsv = nullptr;
        Dc->OMGetRenderTargets( 1, &PrevRtv, &PrevDsv );
        Dc->OMSetRenderTargets( 1, &T->Rtv, nullptr );
        const float Clear[ 4 ] = { 0, 0, 0, 0 };
        Dc->ClearRenderTargetView( T->Rtv, Clear );
        Draw( L, Size, Scale );
        Dc->OMSetRenderTargets( 1, &PrevRtv, PrevDsv );
        SafeRelease( PrevRtv );
        SafeRelease( PrevDsv );
        B.Restore( Dc );
    }

    void D3D11Renderer::Render( const DrawList& L, Vec2 Fb, float Scale )
    {
        if ( !Dev || Fb.x <= 0 || Fb.y <= 0 || L.Idx.empty() ) return;
        Backup B;
        B.Save( Dc );
        Draw( L, Fb, Scale );
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
