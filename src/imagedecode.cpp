// tg::DecodeImage — PNG / JPEG / GIF (first frame) / BMP / TGA bytes -> RGBA (stb_image).
#include "tg/os.h"
#include <algorithm>
#include <cstring>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_PNM
#if defined( __GNUC__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#elif defined( _MSC_VER )
#pragma warning( push, 0 )
#endif
#include "stb_image.h"
#if defined( __GNUC__ )
#pragma GCC diagnostic pop
#elif defined( _MSC_VER )
#pragma warning( pop )
#endif

namespace tg
{
    bool DecodeImage( const void* Data, size_t Size, Bitmap& Out, int MaxSide )
    {
        if ( !Data || !Size || Size > 0x7FFFFFFF ) return false;
        int W = 0, H = 0, N = 0;
        if ( !stbi_info_from_memory( (const stbi_uc*)Data, int( Size ), &W, &H, &N ) ) return false;
        if ( W <= 0 || H <= 0 || ( MaxSide > 0 && ( W > MaxSide || H > MaxSide ) ) ) return false; // refuse decompression bombs
        unsigned char* Px = stbi_load_from_memory( (const stbi_uc*)Data, int( Size ), &W, &H, &N, 4 );
        if ( !Px ) return false;
        Out.Width = W;
        Out.Height = H;
        Out.Pixels.resize( size_t( W ) * size_t( H ) );
        std::memcpy( Out.Pixels.data(), Px, Out.Pixels.size() * 4 );
        stbi_image_free( Px );
        return true;
    }

    Bitmap ScaleBitmap( const Bitmap& In, int W, int H )
    {
        Bitmap O;
        if ( In.Empty() || W <= 0 || H <= 0 ) return O;
        O.Width = W;
        O.Height = H;
        O.Pixels.resize( size_t( W ) * H );
        // box filter when shrinking (avatars from big photos), bilinear-ish nearest when growing
        for ( int y = 0; y < H; y++ )
            for ( int x = 0; x < W; x++ )
            {
                int X0 = x * In.Width / W, X1 = std::max( X0 + 1, ( x + 1 ) * In.Width / W );
                int Y0 = y * In.Height / H, Y1 = std::max( Y0 + 1, ( y + 1 ) * In.Height / H );
                uint32_t R = 0, G = 0, B = 0, A = 0, N = 0;
                for ( int yy = Y0; yy < Y1 && yy < In.Height; yy++ )
                    for ( int xx = X0; xx < X1 && xx < In.Width; xx++ )
                    {
                        uint32_t P = In.Pixels[ size_t( yy ) * In.Width + xx ];
                        uint32_t Pa = P >> 24;
                        R += ( P & 0xFF ) * Pa;
                        G += ( P >> 8 & 0xFF ) * Pa;
                        B += ( P >> 16 & 0xFF ) * Pa;
                        A += Pa;
                        N++;
                    }
                uint32_t Out_ = 0;
                if ( A ) Out_ = ( R / A ) | ( G / A ) << 8 | ( B / A ) << 16 | ( A / N ) << 24;
                O.Pixels[ size_t( y ) * W + x ] = Out_;
            }
        return O;
    }
}
