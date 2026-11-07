// tg/core.h — tiny value types used everywhere. No heavy includes on purpose.
#pragma once
#include <cstdint>
#include <cmath>

namespace tg
{
    struct Vec2
    {
        float x = 0, y = 0;
        constexpr Vec2() = default;
        constexpr Vec2( float X, float Y ) : x( X ), y( Y ) {}
        constexpr Vec2 operator+( Vec2 o ) const { return { x + o.x, y + o.y }; }
        constexpr Vec2 operator-( Vec2 o ) const { return { x - o.x, y - o.y }; }
        constexpr Vec2 operator*( float s ) const { return { x * s, y * s }; }
        constexpr Vec2& operator+=( Vec2 o ) { x += o.x; y += o.y; return *this; }
        constexpr Vec2& operator-=( Vec2 o ) { x -= o.x; y -= o.y; return *this; }
        constexpr bool operator==( const Vec2& ) const = default;
    };

    struct Insets
    {
        float Left = 0, Top = 0, Right = 0, Bottom = 0;
        constexpr Insets() = default;
        constexpr Insets( float All ) : Left( All ), Top( All ), Right( All ), Bottom( All ) {}
        constexpr Insets( float X, float Y ) : Left( X ), Top( Y ), Right( X ), Bottom( Y ) {}
        constexpr Insets( float L, float T, float R, float B ) : Left( L ), Top( T ), Right( R ), Bottom( B ) {}
        constexpr float Horizontal() const { return Left + Right; }
        constexpr float Vertical() const { return Top + Bottom; }
    };

    struct Rect
    {
        float x = 0, y = 0, w = 0, h = 0;
        constexpr Rect() = default;
        constexpr Rect( float X, float Y, float W, float H ) : x( X ), y( Y ), w( W ), h( H ) {}
        constexpr Rect( Vec2 Pos, Vec2 Size ) : x( Pos.x ), y( Pos.y ), w( Size.x ), h( Size.y ) {}

        constexpr Vec2 Pos() const { return { x, y }; }
        constexpr Vec2 Size() const { return { w, h }; }
        constexpr Vec2 Max() const { return { x + w, y + h }; }
        constexpr Vec2 Center() const { return { x + w * 0.5f, y + h * 0.5f }; }
        constexpr float Right() const { return x + w; }
        constexpr float Bottom() const { return y + h; }

        constexpr bool Contains( Vec2 p ) const { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }
        constexpr Rect Shrink( const Insets& i ) const { return { x + i.Left, y + i.Top, w - i.Horizontal(), h - i.Vertical() }; }
        constexpr Rect Shrink( float a ) const { return { x + a, y + a, w - a * 2, h - a * 2 }; }
        constexpr Rect Offset( Vec2 d ) const { return { x + d.x, y + d.y, w, h }; }

        Rect Intersect( const Rect& o ) const
        {
            float X0 = std::fmax( x, o.x ), Y0 = std::fmax( y, o.y );
            float X1 = std::fmin( x + w, o.x + o.w ), Y1 = std::fmin( y + h, o.y + o.h );
            return { X0, Y0, std::fmax( 0.f, X1 - X0 ), std::fmax( 0.f, Y1 - Y0 ) };
        }
        constexpr bool Empty() const { return w <= 0 || h <= 0; }
    };

    // 8-bit straight-alpha RGBA.
    struct Color
    {
        uint8_t r = 0, g = 0, b = 0, a = 255;
        constexpr Color() = default;
        constexpr Color( uint8_t R, uint8_t G, uint8_t B, uint8_t A = 255 ) : r( R ), g( G ), b( B ), a( A ) {}

        // Color::Hex( 0x6EB7D4 ) or Color::Hex( 0x6EB7D4, 128 )
        static constexpr Color Hex( uint32_t Rgb, uint8_t A = 255 )
        {
            return { uint8_t( Rgb >> 16 ), uint8_t( Rgb >> 8 ), uint8_t( Rgb ), A };
        }

        constexpr Color WithAlpha( uint8_t A ) const { return { r, g, b, A }; }
        constexpr Color Fade( float m ) const { return { r, g, b, uint8_t( a * ( m < 0 ? 0 : m > 1 ? 1 : m ) ) }; }

        static constexpr Color Lerp( Color A, Color B, float t )
        {
            auto L = [ t ]( uint8_t x, uint8_t y ) { return uint8_t( x + ( float( y ) - float( x ) ) * t + 0.5f ); };
            return { L( A.r, B.r ), L( A.g, B.g ), L( A.b, B.b ), L( A.a, B.a ) };
        }

        // Packed as bytes R,G,B,A in memory (little-endian u32) — what GPU vertex formats expect.
        constexpr uint32_t Pack() const { return uint32_t( r ) | uint32_t( g ) << 8 | uint32_t( b ) << 16 | uint32_t( a ) << 24; }
        static constexpr Color Unpack( uint32_t c ) { return { uint8_t( c ), uint8_t( c >> 8 ), uint8_t( c >> 16 ), uint8_t( c >> 24 ) }; }

        constexpr bool operator==( const Color& ) const = default;
    };

    // Opaque handle a Renderer backend hands out for a texture (GL name, D3D SRV pointer, ...).
    using TextureId = uintptr_t;

    inline float Clamp( float v, float lo, float hi ) { return v < lo ? lo : v > hi ? hi : v; }
    inline float Lerp( float a, float b, float t ) { return a + ( b - a ) * t; }
}
