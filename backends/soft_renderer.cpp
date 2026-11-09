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

    void SoftRenderer::Render( const DrawList& List, Vec2 Fb, float Scale )
    {
        int W = int( Fb.x ), H = int( Fb.y );
        if ( W != Width || H != Height ) { Width = W; Height = H; }
        Pixels.assign( size_t( W ) * H, 0xFF000000u );

        for ( const DrawCmd& Cmd : List.Cmds )
        {
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
                        if ( T && !T->Px.empty() )
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
        if ( CaptureNext )
        {
            CaptureNext = false;
            Captured.W = W;
            Captured.H = H;
            Captured.Px = Pixels;
        }
    }

    bool SoftRenderer::SavePNG( const std::string& Path ) const
    {
        return stbi_write_png( Path.c_str(), Width, Height, 4, Pixels.data(), Width * 4 ) != 0;
    }
}
