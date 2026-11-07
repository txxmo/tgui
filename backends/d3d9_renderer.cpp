#include "d3d9_renderer.h"
#include "tg/draw.h"
#include <d3d9.h>
#include <algorithm>
#include <cstring>

namespace tg
{
    // DX9 wants D3DCOLOR (BGRA) and a z; we convert per frame.
    struct Vtx9
    {
        float X, Y, Z;
        D3DCOLOR Col;
        float U, V;
    };
    static const DWORD Fvf = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;

    static inline uint32_t RgbaToBgra( uint32_t C ) { return ( C & 0xFF00FF00u ) | ( ( C & 0xFF ) << 16 ) | ( ( C >> 16 ) & 0xFF ); }

    struct D3D9Renderer::Tex
    {
        IDirect3DTexture9* T = nullptr;
        int W = 0, H = 0;
    };

    bool D3D9Renderer::Init( IDirect3DDevice9* D )
    {
        Dev = D;
        Dev->AddRef();
        return CreateDeviceObjects();
    }

    bool D3D9Renderer::CreateDeviceObjects() { return Dev != nullptr; } // buffers are (re)created lazily in Render

    void D3D9Renderer::InvalidateDeviceObjects()
    {
        // D3DPOOL_DEFAULT buffers must go before Reset; textures are D3DPOOL_MANAGED and survive.
        if ( Vb ) { Vb->Release(); Vb = nullptr; }
        if ( Ib ) { Ib->Release(); Ib = nullptr; }
        VbCap = IbCap = 0;
    }

    void D3D9Renderer::Shutdown()
    {
        InvalidateDeviceObjects();
        for ( Tex* T : Textures ) { if ( T->T ) T->T->Release(); delete T; }
        Textures.clear();
        if ( Dev ) { Dev->Release(); Dev = nullptr; }
    }

    bool D3D9Renderer::Upload( Tex& T, int W, int H, const void* Pixels )
    {
        if ( !T.T || T.W != W || T.H != H )
        {
            if ( T.T ) T.T->Release();
            T.T = nullptr;
            if ( FAILED( Dev->CreateTexture( UINT( W ), UINT( H ), 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &T.T, nullptr ) ) ) return false;
            T.W = W;
            T.H = H;
        }
        if ( !Pixels ) return true;
        D3DLOCKED_RECT R;
        if ( FAILED( T.T->LockRect( 0, &R, nullptr, 0 ) ) ) return false;
        const uint32_t* Src = (const uint32_t*)Pixels;
        for ( int y = 0; y < H; y++ )
        {
            uint32_t* Dst = (uint32_t*)( (uint8_t*)R.pBits + size_t( y ) * R.Pitch );
            for ( int x = 0; x < W; x++ ) Dst[ x ] = RgbaToBgra( Src[ size_t( y ) * W + x ] );
        }
        T.T->UnlockRect( 0 );
        return true;
    }

    TextureId D3D9Renderer::CreateTexture( int W, int H, const void* Pixels )
    {
        Tex* T = new Tex();
        Upload( *T, W, H, Pixels );
        Textures.push_back( T );
        return TextureId( T );
    }

    void D3D9Renderer::UpdateTexture( TextureId Id, int W, int H, const void* Pixels )
    {
        if ( Tex* T = (Tex*)Id ) Upload( *T, W, H, Pixels );
    }

    void D3D9Renderer::DestroyTexture( TextureId Id )
    {
        Tex* T = (Tex*)Id;
        auto It = std::find( Textures.begin(), Textures.end(), T );
        if ( It == Textures.end() ) return;
        if ( T->T ) T->T->Release();
        delete T;
        Textures.erase( It );
    }

    void D3D9Renderer::Render( const DrawList& L, Vec2 Fb, float Scale )
    {
        if ( !Dev || Fb.x <= 0 || Fb.y <= 0 || L.Idx.empty() ) return;

        if ( !Vb || VbCap < L.Vtx.size() )
        {
            if ( Vb ) Vb->Release();
            Vb = nullptr;
            VbCap = L.Vtx.size() + 5000;
            if ( FAILED( Dev->CreateVertexBuffer( UINT( VbCap * sizeof( Vtx9 ) ), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, Fvf, D3DPOOL_DEFAULT, &Vb, nullptr ) ) ) { VbCap = 0; return; }
        }
        if ( !Ib || IbCap < L.Idx.size() )
        {
            if ( Ib ) Ib->Release();
            Ib = nullptr;
            IbCap = L.Idx.size() + 10000;
            if ( FAILED( Dev->CreateIndexBuffer( UINT( IbCap * sizeof( uint32_t ) ), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &Ib, nullptr ) ) ) { IbCap = 0; return; }
        }

        // Positions go out in framebuffer pixels (UI units * Scale): DX9 has no shader here to scale for us.
        Vtx9* V = nullptr;
        if ( FAILED( Vb->Lock( 0, UINT( L.Vtx.size() * sizeof( Vtx9 ) ), (void**)&V, D3DLOCK_DISCARD ) ) ) return;
        for ( const Vertex& S : L.Vtx )
            *V++ = { S.Pos.x * Scale, S.Pos.y * Scale, 0.f, RgbaToBgra( S.Col ), S.UV.x, S.UV.y };
        Vb->Unlock();
        void* I = nullptr;
        if ( FAILED( Ib->Lock( 0, UINT( L.Idx.size() * sizeof( uint32_t ) ), &I, D3DLOCK_DISCARD ) ) ) return;
        std::memcpy( I, L.Idx.data(), L.Idx.size() * sizeof( uint32_t ) );
        Ib->Unlock();

        IDirect3DStateBlock9* Saved = nullptr;
        if ( FAILED( Dev->CreateStateBlock( D3DSBT_ALL, &Saved ) ) ) return;
        Saved->Capture();
        D3DMATRIX Last[ 3 ];
        Dev->GetTransform( D3DTS_WORLD, &Last[ 0 ] );
        Dev->GetTransform( D3DTS_VIEW, &Last[ 1 ] );
        Dev->GetTransform( D3DTS_PROJECTION, &Last[ 2 ] );

        D3DVIEWPORT9 Vp = { 0, 0, DWORD( Fb.x ), DWORD( Fb.y ), 0.f, 1.f };
        Dev->SetViewport( &Vp );
        Dev->SetPixelShader( nullptr );
        Dev->SetVertexShader( nullptr );
        Dev->SetFVF( Fvf );
        Dev->SetStreamSource( 0, Vb, 0, sizeof( Vtx9 ) );
        Dev->SetIndices( Ib );
        Dev->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
        Dev->SetRenderState( D3DRS_SHADEMODE, D3DSHADE_GOURAUD );
        Dev->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
        Dev->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
        Dev->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
        Dev->SetRenderState( D3DRS_ZENABLE, FALSE );
        Dev->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        Dev->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
        Dev->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
        Dev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
        Dev->SetRenderState( D3DRS_SEPARATEALPHABLENDENABLE, TRUE );
        Dev->SetRenderState( D3DRS_SRCBLENDALPHA, D3DBLEND_ONE );
        Dev->SetRenderState( D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA );
        Dev->SetRenderState( D3DRS_SCISSORTESTENABLE, TRUE );
        Dev->SetRenderState( D3DRS_FOGENABLE, FALSE );
        Dev->SetRenderState( D3DRS_RANGEFOGENABLE, FALSE );
        Dev->SetRenderState( D3DRS_SPECULARENABLE, FALSE );
        Dev->SetRenderState( D3DRS_STENCILENABLE, FALSE );
        Dev->SetRenderState( D3DRS_CLIPPING, TRUE );
        Dev->SetRenderState( D3DRS_LIGHTING, FALSE );
        Dev->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
        Dev->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
        Dev->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
        Dev->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
        Dev->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
        Dev->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
        Dev->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
        Dev->SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
        Dev->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
        Dev->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
        Dev->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
        Dev->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

        // Pixel-space orthographic projection with DX9's half-pixel offset.
        {
            float Lf = 0.5f, R = Fb.x + 0.5f, T = 0.5f, B = Fb.y + 0.5f;
            D3DMATRIX Id = { { { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } } };
            D3DMATRIX P = { { { 2.f / ( R - Lf ), 0, 0, 0, 0, 2.f / ( T - B ), 0, 0, 0, 0, 0.5f, 0, ( Lf + R ) / ( Lf - R ), ( T + B ) / ( B - T ), 0.5f, 1 } } };
            Dev->SetTransform( D3DTS_WORLD, &Id );
            Dev->SetTransform( D3DTS_VIEW, &Id );
            Dev->SetTransform( D3DTS_PROJECTION, &P );
        }

        for ( const DrawCmd& C : L.Cmds )
        {
            if ( !C.IdxCount ) continue;
            RECT R = { LONG( C.Clip.x * Scale ), LONG( C.Clip.y * Scale ), LONG( C.Clip.Right() * Scale + 0.5f ), LONG( C.Clip.Bottom() * Scale + 0.5f ) };
            if ( R.right <= R.left || R.bottom <= R.top ) continue;
            Dev->SetScissorRect( &R );
            Tex* T = (Tex*)C.Texture;
            Dev->SetTexture( 0, T ? T->T : nullptr );
            Dev->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, UINT( L.Vtx.size() ), C.IdxOffset, C.IdxCount / 3 );
        }

        Dev->SetTransform( D3DTS_WORLD, &Last[ 0 ] );
        Dev->SetTransform( D3DTS_VIEW, &Last[ 1 ] );
        Dev->SetTransform( D3DTS_PROJECTION, &Last[ 2 ] );
        Saved->Apply();
        Saved->Release();
    }
}
