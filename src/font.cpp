#include "tg/font.h"
#include "tg/renderer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace tg
{
    uint32_t DecodeUtf8( std::string_view s, size_t& i )
    {
        unsigned char c = s[ i++ ];
        if ( c < 0x80 ) return c;
        int Extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
        uint32_t Cp = c & ( 0x3F >> Extra );
        for ( int k = 0; k < Extra && i < s.size(); k++ )
        {
            unsigned char n = s[ i ];
            if ( ( n & 0xC0 ) != 0x80 ) return 0xFFFD;
            Cp = ( Cp << 6 ) | ( n & 0x3F );
            i++;
        }
        return Extra ? Cp : 0xFFFD;
    }

    // ------------------------------------------------------------ Font

    const Glyph& Font::Find( uint32_t Cp ) const
    {
        auto It = Glyphs.find( Cp );
        return It != Glyphs.end() ? It->second : Fallback;
    }

    float Font::Kern( uint32_t A, uint32_t B ) const
    {
        if ( !Kerning || !A || !Info ) return 0;
        const Glyph& Ga = Find( A );
        const Glyph& Gb = Find( B );
        if ( !Ga.Index || !Gb.Index ) return 0;
        uint64_t Key = ( uint64_t( uint32_t( Ga.Index ) ) << 32 ) | uint32_t( Gb.Index );
        auto It = KernCache.find( Key );
        if ( It != KernCache.end() ) return It->second;
        auto* I = static_cast<const stbtt_fontinfo*>( Info.get() );
        float K = stbtt_GetGlyphKernAdvance( I, Ga.Index, Gb.Index ) * KernScale;
        KernCache.emplace( Key, K );
        return K;
    }

    Vec2 Font::Measure( std::string_view Str, float WrapWidth ) const
    {
        Vec2 Out;
        ForEachLine( Str, WrapWidth, [ & ]( std::string_view Line, float Y )
        {
            float X = 0;
            uint32_t Prev = 0;
            for ( size_t i = 0; i < Line.size(); )
            {
                uint32_t Cp = DecodeUtf8( Line, i );
                X += Kern( Prev, Cp ) + Find( Cp ).Advance;
                Prev = Cp;
            }
            Out.x = std::max( Out.x, X );
            Out.y = Y + LineHeight;
        } );
        return Out;
    }

    size_t Font::HitTest( std::string_view Str, float X ) const
    {
        float Pen = 0;
        uint32_t Prev = 0;
        for ( size_t i = 0; i < Str.size(); )
        {
            size_t Start = i;
            uint32_t Cp = DecodeUtf8( Str, i );
            Pen += Kern( Prev, Cp );
            Prev = Cp;
            float Adv = Find( Cp ).Advance;
            if ( X < Pen + Adv * 0.5f ) return Start;
            Pen += Adv;
        }
        return Str.size();
    }

    // ------------------------------------------------------------ FontAtlas

    FontAtlas::FontAtlas() = default;
    FontAtlas::~FontAtlas() = default;

    Font* FontAtlas::AddFromMemory( std::vector<unsigned char> Data, float PixelSize )
    {
        Fonts.push_back( std::make_unique<Font>() );
        Font* F = Fonts.back().get();
        F->Size = PixelSize;
        Sources.push_back( { std::make_shared<std::vector<unsigned char>>( std::move( Data ) ), PixelSize, F } );
        return F;
    }

    Font* FontAtlas::AddFromFile( const std::string& Path, float PixelSize )
    {
        FILE* Fp = std::fopen( Path.c_str(), "rb" );
        if ( !Fp ) return nullptr;
        std::fseek( Fp, 0, SEEK_END );
        long Len = std::ftell( Fp );
        std::fseek( Fp, 0, SEEK_SET );
        std::vector<unsigned char> Data( size_t( Len > 0 ? Len : 0 ) );
        size_t Got = std::fread( Data.data(), 1, Data.size(), Fp );
        std::fclose( Fp );
        if ( Got != Data.size() || Data.empty() ) return nullptr;
        return AddFromMemory( std::move( Data ), PixelSize );
    }

    // Which codepoints we bake. Anything missing from a font is skipped (renders as '?').
    static void CollectCodepoints( std::vector<int>& Out )
    {
        auto Range = [ & ]( int A, int B ) { for ( int c = A; c <= B; c++ ) Out.push_back( c ); };
        Range( 0x20, 0x7E );     // ASCII
        Range( 0xA0, 0xFF );     // Latin-1
        Range( 0x2010, 0x2044 ); // dashes, quotes, bullet, ellipsis, ...
        Range( 0x2190, 0x2193 ); // arrows
        Range( 0x2212, 0x2212 ); // minus
        Range( 0x25B2, 0x25BC ); // triangles
        Range( 0x2713, 0x2717 ); // check marks / crosses
    }

    bool FontAtlas::Build( Renderer& R, float Scale )
    {
        std::vector<int> Cps;
        CollectCodepoints( Cps );

        const int Reserved = 4; // top rows: a 2x2 white block for solid shapes
        Width = 1024;
        for ( int H = 256; H <= 4096; H *= 2 )
        {
            std::vector<unsigned char> Alpha( size_t( Width ) * ( H + Reserved ), 0 );
            stbtt_pack_context Pc;
            if ( !stbtt_PackBegin( &Pc, Alpha.data() + Width * Reserved, Width, H, Width, 1, nullptr ) ) return false;
            const int Oh = std::clamp( Quality.OversampleH, 1, 8 ), Ov = std::clamp( Quality.OversampleV, 1, 8 );
            stbtt_PackSetOversampling( &Pc, unsigned( Oh ), unsigned( Ov ) );

            bool Ok = true;
            struct Baked { std::vector<int> Cps; std::vector<stbtt_packedchar> Chars; std::shared_ptr<stbtt_fontinfo> Info; float Px = 0; };
            std::vector<Baked> Results( Sources.size() );

            for ( size_t s = 0; s < Sources.size() && Ok; s++ )
            {
                const auto& Src = Sources[ s ];
                const unsigned char* Data = Src.Data->data();
                // The fontinfo points into the font bytes; keep both alive together (kerning queries later).
                auto Keep = Src.Data;
                std::shared_ptr<stbtt_fontinfo> InfoP( new stbtt_fontinfo(), [ Keep ]( stbtt_fontinfo* P ) { delete P; } );
                stbtt_fontinfo& Info = *InfoP;
                if ( !stbtt_InitFont( &Info, Data, stbtt_GetFontOffsetForIndex( Data, 0 ) ) ) { Ok = false; break; }

                Baked& B = Results[ s ];
                B.Info = InfoP;
                for ( int c : Cps )
                    if ( stbtt_FindGlyphIndex( &Info, c ) ) B.Cps.push_back( c );
                B.Chars.resize( B.Cps.size() );

                stbtt_pack_range Range{};
                Range.font_size = Src.Size * Scale;
                Range.array_of_unicode_codepoints = B.Cps.data();
                Range.num_chars = int( B.Cps.size() );
                Range.chardata_for_range = B.Chars.data();
                if ( !stbtt_PackFontRanges( &Pc, Data, 0, &Range, 1 ) ) Ok = false;

                // Metrics (in UI units).
                float Px = stbtt_ScaleForPixelHeight( &Info, Src.Size * Scale );
                B.Px = Px;
                int Asc, Desc, Gap;
                stbtt_GetFontVMetrics( &Info, &Asc, &Desc, &Gap );
                Font* F = Src.Out;
                F->Ascent = std::round( Asc * Px ) / Scale;
                F->LineHeight = std::round( ( Asc - Desc + Gap ) * Px ) / Scale;
            }
            stbtt_PackEnd( &Pc );
            if ( !Ok ) continue; // didn't fit: try a taller atlas

            Height = H + Reserved;
            // Coverage curve: stb's coverage is linear, which makes small text look thin, especially
            // light-on-dark. Gamma > 1 lifts partial coverage so strokes read solid without going bold.
            uint8_t Curve[ 256 ];
            for ( int a = 0; a < 256; a++ )
                Curve[ a ] = uint8_t( std::lround( 255.0 * std::pow( a / 255.0, 1.0 / std::max( 0.1f, Quality.Gamma ) ) ) );
            Pixels.assign( size_t( Width ) * Height, 0 );
            for ( size_t i = 0; i < Pixels.size(); i++ )
                Pixels[ i ] = Color( 255, 255, 255, Curve[ Alpha[ i ] ] ).Pack();
            for ( int y = 1; y <= 2; y++ )
                for ( int x = 1; x <= 2; x++ )
                    Pixels[ size_t( y ) * Width + x ] = 0xFFFFFFFFu;
            WhiteUV = { 2.f / Width, 2.f / Height };

            for ( size_t s = 0; s < Sources.size(); s++ )
            {
                Font* F = Sources[ s ].Out;
                F->Glyphs.clear();
                const Baked& B = Results[ s ];
                F->Info = B.Info;
                F->KernScale = B.Px / Scale;
                F->Kerning = Quality.Kerning;
                F->KernCache.clear();
                F->SubpixelSteps = float( Oh );
                for ( size_t i = 0; i < B.Cps.size(); i++ )
                {
                    const stbtt_packedchar& P = B.Chars[ i ];
                    Glyph G;
                    G.X0 = P.xoff / Scale;  G.Y0 = P.yoff / Scale;
                    G.X1 = P.xoff2 / Scale; G.Y1 = P.yoff2 / Scale;
                    G.U0 = P.x0 / float( Width );
                    G.V0 = ( P.y0 + Reserved ) / float( Height );
                    G.U1 = P.x1 / float( Width );
                    G.V1 = ( P.y1 + Reserved ) / float( Height );
                    G.Advance = P.xadvance / Scale;
                    G.Index = stbtt_FindGlyphIndex( B.Info.get(), B.Cps[ i ] );
                    F->Glyphs[ uint32_t( B.Cps[ i ] ) ] = G;
                }
                F->Fallback = F->Find( '?' );
            }

            if ( Texture ) R.DestroyTexture( Texture );
            Texture = R.CreateTexture( Width, Height, Pixels.data() );
            return true;
        }
        return false;
    }
}
