#include "soft_renderer.h"
#include "tg/draw.h"
#include <algorithm>
#include <cmath>
#include <cstring>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace tg
{
    TextureId SoftRenderer::CreateTexture( int W, int H, const void* P )
    {
        TextureId Id = NextId++;
        UpdateTexture( Id, W, H, P );
        return Id;
    }

    void SoftRenderer::UpdateTexture( TextureId Id, int W, int H, const void* P )
    {
        Tex& T = Textures[ Id ];
        T.W = W;
        T.H = H;
        T.Px.resize( size_t( W ) * H );
        if ( P ) std::memcpy( T.Px.data(), P, T.Px.size() * 4 );
    }

    void SoftRenderer::DestroyTexture( TextureId Id ) { Textures.erase( Id ); }

    namespace
    {
        struct RGBAf { float r, g, b, a; };

        RGBAf Unpack( uint32_t c ) { return { float( c & 255 ), float( ( c >> 8 ) & 255 ), float( ( c >> 16 ) & 255 ), float( c >> 24 ) }; }

        RGBAf Sample( const std::vector<uint32_t>& Px, int W, int H, float U, float V )
        {
            float X = U * W - 0.5f, Y = V * H - 0.5f;
            int X0 = int( std::floor( X ) ), Y0 = int( std::floor( Y ) );
            float Fx = X - X0, Fy = Y - Y0;
            auto At = [ & ]( int x, int y ) { x = std::clamp( x, 0, W - 1 ); y = std::clamp( y, 0, H - 1 ); return Unpack( Px[ size_t( y ) * W + x ] ); };
            RGBAf A = At( X0, Y0 ), B = At( X0 + 1, Y0 ), C = At( X0, Y0 + 1 ), D = At( X0 + 1, Y0 + 1 );
            auto L = [ & ]( float a, float b, float c, float d ) { return ( a + ( b - a ) * Fx ) + ( ( c + ( d - c ) * Fx ) - ( a + ( b - a ) * Fx ) ) * Fy; };
            return { L( A.r, B.r, C.r, D.r ), L( A.g, B.g, C.g, D.g ), L( A.b, B.b, C.b, D.b ), L( A.a, B.a, C.a, D.a ) };
        }
    }

    ShaderId SoftRenderer::CreateShader( const ShaderDesc& D )
    {
        if ( !D.Cpu ) return 0; // no C++ version: this renderer can't run it
        Shaders[ NextShader ] = D;
        return NextShader++;
    }

    void SoftRenderer::DestroyShader( ShaderId S ) { Shaders.erase( S ); }

    TextureId SoftRenderer::CreateRenderTarget( int W, int H ) { return CreateTexture( W, H, nullptr ); }

    void SoftRenderer::RenderTo( TextureId Target, const DrawList& List, Vec2 Size, float Scale )
    {
        auto It = Textures.find( Target );
        if ( It == Textures.end() ) return;
        Tex& T = It->second;
        int W = int( Size.x ), H = int( Size.y );
        if ( T.W != W || T.H != H ) { T.W = W; T.H = H; }
        std::vector<uint32_t> Px( size_t( W ) * H, 0u ); // transparent
        Raster( List, Scale, Px, W, H );
        T.Px = std::move( Px ); // (sampling never reads the target it's drawing into)
    }

    // A rounded-rect mask: 1 inside, 0 outside (pixel units).
    static bool InRounded( float X, float Y, Rect R, float Rad )
    {
        if ( !R.Contains( { X, Y } ) ) return false;
        float Cx = std::clamp( X, R.x + Rad, R.Right() - Rad ), Cy = std::clamp( Y, R.y + Rad, R.Bottom() - Rad );
        float Dx = X - Cx, Dy = Y - Cy;
        return Dx * Dx + Dy * Dy <= Rad * Rad;
    }

    // Three box blurs ≈ a Gaussian; done in place on the region behind a frosted panel.
    static void BlurRegion( std::vector<uint32_t>& Px, int W, int H, Rect Area, float Amount, float Radius )
    {
        int X0 = std::max( 0, int( Area.x ) ), Y0 = std::max( 0, int( Area.y ) );
        int X1 = std::min( W, int( std::ceil( Area.Right() ) ) ), Y1 = std::min( H, int( std::ceil( Area.Bottom() ) ) );
        if ( X0 >= X1 || Y0 >= Y1 ) return;
        int Pad = int( Amount ) + 2;
        int Rx0 = std::max( 0, X0 - Pad ), Ry0 = std::max( 0, Y0 - Pad ), Rx1 = std::min( W, X1 + Pad ), Ry1 = std::min( H, Y1 + Pad );
        int Rw = Rx1 - Rx0, Rh = Ry1 - Ry0;
        std::vector<RGBAf> A( size_t( Rw ) * Rh ), B( A.size() );
        for ( int y = 0; y < Rh; y++ )
            for ( int x = 0; x < Rw; x++ ) A[ size_t( y ) * Rw + x ] = Unpack( Px[ size_t( y + Ry0 ) * W + x + Rx0 ] );
        int R = std::max( 1, int( Amount / 3 ) );
        for ( int Pass = 0; Pass < 3; Pass++ )
            for ( int Axis = 0; Axis < 2; Axis++ )
            {
                int N = Axis == 0 ? Rw : Rh, M = Axis == 0 ? Rh : Rw;
                for ( int m = 0; m < M; m++ )
                {
                    auto At = [ & ]( int i ) -> RGBAf& { i = std::clamp( i, 0, N - 1 ); return Axis == 0 ? A[ size_t( m ) * Rw + i ] : A[ size_t( i ) * Rw + m ]; };
                    RGBAf Sum{ 0, 0, 0, 0 };
                    for ( int i = -R; i <= R; i++ ) { RGBAf V = At( i ); Sum.r += V.r; Sum.g += V.g; Sum.b += V.b; Sum.a += V.a; }
                    float K = 1.f / ( 2 * R + 1 );
                    for ( int i = 0; i < N; i++ )
                    {
                        RGBAf& O = Axis == 0 ? B[ size_t( m ) * Rw + i ] : B[ size_t( i ) * Rw + m ];
                        O = { Sum.r * K, Sum.g * K, Sum.b * K, Sum.a * K };
                        RGBAf Add = At( i + R + 1 ), Sub = At( i - R );
                        Sum.r += Add.r - Sub.r; Sum.g += Add.g - Sub.g; Sum.b += Add.b - Sub.b; Sum.a += Add.a - Sub.a;
                    }
                }
                A.swap( B );
            }
        for ( int y = Y0; y < Y1; y++ )
            for ( int x = X0; x < X1; x++ )
                if ( InRounded( x + 0.5f, y + 0.5f, Area, Radius ) )
                {
                    RGBAf C = A[ size_t( y - Ry0 ) * Rw + ( x - Rx0 ) ];
                    auto Q = []( float V ) { return uint32_t( std::clamp( V + 0.5f, 0.f, 255.f ) ); };
                    Px[ size_t( y ) * W + x ] = Q( C.r ) | Q( C.g ) << 8 | Q( C.b ) << 16 | Q( C.a ) << 24;
                }
    }

    void SoftRenderer::Render( const DrawList& List, Vec2 Fb, float Scale )
    {
        int W = int( Fb.x ), H = int( Fb.y );
        if ( W != Width || H != Height ) { Width = W; Height = H; }
        Pixels.assign( size_t( W ) * H, 0xFF000000u );
        Raster( List, Scale, Pixels, W, H );
        if ( CaptureNext )
        {
            CaptureNext = false;
            Captured.W = W;
            Captured.H = H;
            Captured.Px = Pixels;
        }
    }

    void SoftRenderer::Raster( const DrawList& List, float Scale, std::vector<uint32_t>& Pixels, int W, int H )
    {
        Time = List.Time;
        for ( const DrawCmd& Cmd : List.Cmds )
        {
            if ( Cmd.Kind == DrawCmd::Backdrop )
            {
                Rect A = { Cmd.Area.x * Scale, Cmd.Area.y * Scale, Cmd.Area.w * Scale, Cmd.Area.h * Scale };
                BlurRegion( Pixels, W, H, A.Intersect( { Cmd.Clip.x * Scale, Cmd.Clip.y * Scale, Cmd.Clip.w * Scale, Cmd.Clip.h * Scale } ), Cmd.Amount * Scale, Cmd.Radius * Scale );
                continue;
            }
            const ShaderDesc* Sh = nullptr;
            if ( Cmd.Shader )
            {
                auto Si = Shaders.find( Cmd.Shader );
                if ( Si != Shaders.end() ) Sh = &Si->second;
            }
            const float* Params = Cmd.ParamOffset + 32 <= List.Params.size() ? List.Params.data() + Cmd.ParamOffset : nullptr;
            auto TexIt = Textures.find( Cmd.Texture );
            const Tex* T = TexIt != Textures.end() ? &TexIt->second : nullptr;

            int Cx0 = std::max( 0, int( std::floor( Cmd.Clip.x * Scale ) ) );
            int Cy0 = std::max( 0, int( std::floor( Cmd.Clip.y * Scale ) ) );
            int Cx1 = std::min( W, int( std::ceil( Cmd.Clip.Right() * Scale ) ) );
            int Cy1 = std::min( H, int( std::ceil( Cmd.Clip.Bottom() * Scale ) ) );
            if ( Cx0 >= Cx1 || Cy0 >= Cy1 ) continue;

            for ( uint32_t i = 0; i < Cmd.IdxCount; i += 3 )
            {
                const Vertex* V[ 3 ] = { &List.Vtx[ List.Idx[ Cmd.IdxOffset + i ] ], &List.Vtx[ List.Idx[ Cmd.IdxOffset + i + 1 ] ], &List.Vtx[ List.Idx[ Cmd.IdxOffset + i + 2 ] ] };
                Vec2 P[ 3 ] = { V[ 0 ]->Pos * Scale, V[ 1 ]->Pos * Scale, V[ 2 ]->Pos * Scale };

                float Area = ( P[ 1 ].x - P[ 0 ].x ) * ( P[ 2 ].y - P[ 0 ].y ) - ( P[ 1 ].y - P[ 0 ].y ) * ( P[ 2 ].x - P[ 0 ].x );
                if ( std::fabs( Area ) < 1e-8f ) continue;
                if ( Area < 0 ) { std::swap( P[ 1 ], P[ 2 ] ); std::swap( V[ 1 ], V[ 2 ] ); Area = -Area; }

                int X0 = std::max( Cx0, int( std::floor( std::min( { P[ 0 ].x, P[ 1 ].x, P[ 2 ].x } ) ) ) );
                int X1 = std::min( Cx1, int( std::ceil( std::max( { P[ 0 ].x, P[ 1 ].x, P[ 2 ].x } ) ) ) );
                int Y0 = std::max( Cy0, int( std::floor( std::min( { P[ 0 ].y, P[ 1 ].y, P[ 2 ].y } ) ) ) );
                int Y1 = std::min( Cy1, int( std::ceil( std::max( { P[ 0 ].y, P[ 1 ].y, P[ 2 ].y } ) ) ) );

                RGBAf Col[ 3 ] = { Unpack( V[ 0 ]->Col ), Unpack( V[ 1 ]->Col ), Unpack( V[ 2 ]->Col ) };

                // Edge k is opposite vertex k. Tie-break so pixels on a shared edge are drawn exactly once.
                auto EdgeFn = [ & ]( Vec2 a, Vec2 b, float px, float py ) { return ( b.x - a.x ) * ( py - a.y ) - ( b.y - a.y ) * ( px - a.x ); };
                auto Owns = [ & ]( Vec2 a, Vec2 b ) { float dx = b.x - a.x, dy = b.y - a.y; return dy > 0 || ( dy == 0 && dx < 0 ); };
                bool Own[ 3 ] = { Owns( P[ 1 ], P[ 2 ] ), Owns( P[ 2 ], P[ 0 ] ), Owns( P[ 0 ], P[ 1 ] ) };

                for ( int y = Y0; y < Y1; y++ )
                {
                    float Py = y + 0.5f;
                    for ( int x = X0; x < X1; x++ )
                    {
                        float Px = x + 0.5f;
                        float E0 = EdgeFn( P[ 1 ], P[ 2 ], Px, Py ), E1 = EdgeFn( P[ 2 ], P[ 0 ], Px, Py ), E2 = EdgeFn( P[ 0 ], P[ 1 ], Px, Py );
                        if ( E0 < 0 || E1 < 0 || E2 < 0 ) continue;
                        if ( ( E0 == 0 && !Own[ 0 ] ) || ( E1 == 0 && !Own[ 1 ] ) || ( E2 == 0 && !Own[ 2 ] ) ) continue;

                        float B0 = E0 / Area, B1 = E1 / Area, B2 = E2 / Area;
                        RGBAf C = { Col[ 0 ].r * B0 + Col[ 1 ].r * B1 + Col[ 2 ].r * B2, Col[ 0 ].g * B0 + Col[ 1 ].g * B1 + Col[ 2 ].g * B2,
                                    Col[ 0 ].b * B0 + Col[ 1 ].b * B1 + Col[ 2 ].b * B2, Col[ 0 ].a * B0 + Col[ 1 ].a * B1 + Col[ 2 ].a * B2 };
                        if ( Sh )
                        {
                            float U = V[ 0 ]->UV.x * B0 + V[ 1 ]->UV.x * B1 + V[ 2 ]->UV.x * B2;
                            float Vv = V[ 0 ]->UV.y * B0 + V[ 1 ]->UV.y * B1 + V[ 2 ]->UV.y * B2;
                            static const float Zero[ 32 ] = {};
                            auto Tx = [ T ]( Vec2 Uv ) {
                                if ( !T || T->Px.empty() ) return Color( 255, 255, 255 );
                                RGBAf S = Sample( T->Px, T->W, T->H, Uv.x, Uv.y );
                                return Color( uint8_t( S.r ), uint8_t( S.g ), uint8_t( S.b ), uint8_t( S.a ) );
                            };
                            Color In( uint8_t( std::clamp( C.r, 0.f, 255.f ) ), uint8_t( std::clamp( C.g, 0.f, 255.f ) ), uint8_t( std::clamp( C.b, 0.f, 255.f ) ), uint8_t( std::clamp( C.a, 0.f, 255.f ) ) );
                            Color O = Sh->Cpu( In, { U, Vv }, { Px, Py }, Params ? Params : Zero, Tx );
                            C = { float( O.r ), float( O.g ), float( O.b ), float( O.a ) };
                        }
                        else if ( T && !T->Px.empty() )
                        {
                            float U = V[ 0 ]->UV.x * B0 + V[ 1 ]->UV.x * B1 + V[ 2 ]->UV.x * B2;
                            float Vv = V[ 0 ]->UV.y * B0 + V[ 1 ]->UV.y * B1 + V[ 2 ]->UV.y * B2;
                            RGBAf S = Sample( T->Px, T->W, T->H, U, Vv );
                            C = { C.r * S.r / 255, C.g * S.g / 255, C.b * S.b / 255, C.a * S.a / 255 };
                        }
                        float A = C.a / 255.f;
                        if ( A <= 0 ) continue;
                        uint32_t& Dst = Pixels[ size_t( y ) * W + x ];
                        RGBAf D = Unpack( Dst );
                        auto Mix = [ A ]( float s, float d ) { return uint32_t( std::clamp( s * A + d * ( 1 - A ) + 0.5f, 0.f, 255.f ) ); };
                        uint32_t Oa = uint32_t( std::clamp( C.a + D.a * ( 1 - A ) + 0.5f, 0.f, 255.f ) );
                        Dst = Mix( C.r, D.r ) | Mix( C.g, D.g ) << 8 | Mix( C.b, D.b ) << 16 | Oa << 24;
                    }
                }
            }
        }
    }

    bool SoftRenderer::SavePNG( const std::string& Path ) const
    {
        return stbi_write_png( Path.c_str(), Width, Height, 4, Pixels.data(), Width * 4 ) != 0;
    }
}
