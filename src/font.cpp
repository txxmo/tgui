#include "tg/font.h"
#include "tg/renderer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#ifdef TG_HAS_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif
#ifdef TG_HAS_HARFBUZZ
#include <hb.h>
#endif

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

    // Codepoints that take no space on their own: joiners and selectors inside emoji sequences, skin-tone
    // modifiers (we draw the base emoji), tag characters. Without a shaper they'd show up as boxes.
    static bool IsZeroWidth( uint32_t Cp )
    {
        return Cp == 0x200B || Cp == 0x200C || Cp == 0x200D || Cp == 0xFE0E || Cp == 0xFE0F || Cp == 0x20E3 ||
               ( Cp >= 0x1F3FB && Cp <= 0x1F3FF ) || ( Cp >= 0xE0020 && Cp <= 0xE007F );
    }

    // ------------------------------------------------------------ emoji sequences

    static bool IsRegional( uint32_t C ) { return C >= 0x1F1E6 && C <= 0x1F1FF; }
    static bool IsEmojiModifier( uint32_t C ) { return C == 0xFE0F || ( C >= 0x1F3FB && C <= 0x1F3FF ) || ( C >= 0xE0020 && C <= 0xE007F ); }
    static bool IsEmojiBase( uint32_t C )
    {
        return C >= 0x1F000 || ( C >= 0x2300 && C <= 0x2BFF ) || C == 0xA9 || C == 0xAE || C == 0x203C || C == 0x2049 ||
               C == 0x2122 || C == 0x2139 || ( C >= 0x2194 && C <= 0x21AA ) || C == 0x3030 || C == 0x303D || C == 0x3297 || C == 0x3299;
    }

    size_t EmojiSequenceLength( std::string_view S, size_t i )
    {
        if ( i >= S.size() ) return 0;
        size_t j = i;
        uint32_t C = DecodeUtf8( S, j );
        auto Peek = [ & ]( size_t At, size_t& After ) -> uint32_t { if ( At >= S.size() ) return 0; After = At; return DecodeUtf8( S, After ); };
        size_t N = 0;
        if ( IsRegional( C ) ) // flag: exactly two regional indicators
            return IsRegional( Peek( j, N ) ) ? N - i : 0;
        if ( ( C >= '0' && C <= '9' ) || C == '#' || C == '*' ) // keycap: base [FE0F] 20E3
        {
            uint32_t D = Peek( j, N );
            if ( D == 0xFE0F ) { j = N; D = Peek( j, N ); }
            return D == 0x20E3 ? N - i : 0;
        }
        if ( !IsEmojiBase( C ) ) return 0;
        int Count = 1;
        for ( ;; )
        {
            uint32_t D = Peek( j, N );
            if ( IsEmojiModifier( D ) ) { j = N; Count++; continue; }
            if ( D == 0x200D ) // ZWJ: joins only if another emoji follows
            {
                size_t M = 0;
                uint32_t E = Peek( N, M );
                if ( IsEmojiBase( E ) ) { j = M; Count += 2; continue; }
            }
            break;
        }
        return Count > 1 ? j - i : 0;
    }

    size_t NextGrapheme( std::string_view S, size_t i )
    {
        if ( i >= S.size() ) return S.size();
        if ( size_t L = EmojiSequenceLength( S, i ) ) return i + L;
        DecodeUtf8( S, i );
        while ( i < S.size() ) // combining marks, selectors, modifiers stay with their base
        {
            size_t j = i;
            uint32_t C = DecodeUtf8( S, j );
            if ( !( IsEmojiModifier( C ) || ( C >= 0x300 && C <= 0x36F ) || C == 0x20E3 ) ) break;
            i = j;
        }
        return i;
    }

    size_t PrevGrapheme( std::string_view S, size_t i )
    {
        size_t Line = i >= 2 ? S.rfind( '\n', i - 2 ) : std::string_view::npos; // walk from the line start, not the text start
        size_t At = Line == std::string_view::npos ? 0 : Line + 1, Last = At;
        while ( At < i && At < S.size() ) { Last = At; At = NextGrapheme( S, At ); }
        return Last;
    }

    // ------------------------------------------------------------ internals

#ifdef TG_HAS_FREETYPE
    static FT_Library FtLib()
    {
        static FT_Library L = [] { FT_Library X = nullptr; FT_Init_FreeType( &X ); return X; }();
        return L;
    }
#endif

    struct FontAtlas::Source
    {
        std::shared_ptr<std::vector<unsigned char>> Data;
        float Size = 0;
        Font* Out = nullptr;
        std::shared_ptr<stbtt_fontinfo> Info; // null for fonts stb can't read (bitmap-only color fonts)
        bool Color = false;                   // has color glyphs (emoji)
        std::string LazyPath;                 // big fallbacks (CJK, ~20 MB) are read on first use
#ifdef TG_HAS_FREETYPE
        FT_Face Face = nullptr;
#endif
#ifdef TG_HAS_HARFBUZZ
        hb_font_t* Hb = nullptr; // created on first use, reads the same bytes as FreeType
        hb_font_t* Shaper()
        {
            if ( !Hb && Data )
            {
                hb_blob_t* Blob = hb_blob_create( (const char*)Data->data(), unsigned( Data->size() ), HB_MEMORY_MODE_READONLY, nullptr, nullptr );
                hb_face_t* F = hb_face_create( Blob, 0 );
                Hb = hb_font_create( F );
                hb_face_destroy( F );
                hb_blob_destroy( Blob );
            }
            return Hb;
        }
#endif
        ~Source()
        {
#ifdef TG_HAS_HARFBUZZ
            if ( Hb ) hb_font_destroy( Hb );
#endif
#ifdef TG_HAS_FREETYPE
            if ( Face ) FT_Done_Face( Face );
#endif
        }
        bool Open()
        {
            const unsigned char* D = Data->data();
            auto Keep = Data;
            auto I = std::shared_ptr<stbtt_fontinfo>( new stbtt_fontinfo(), [ Keep ]( stbtt_fontinfo* P ) { delete P; } );
            int Off = stbtt_GetFontOffsetForIndex( D, 0 );
            if ( Off >= 0 && stbtt_InitFont( I.get(), D, Off ) ) Info = I;
#ifdef TG_HAS_FREETYPE
            if ( FtLib() && FT_New_Memory_Face( FtLib(), D, FT_Long( Data->size() ), 0, &Face ) == 0 )
                Color = FT_HAS_COLOR( Face );
            else
                Face = nullptr;
            if ( Color ) Info = nullptr; // emoji fonts: always FreeType (stb can't draw color glyphs)
            return Info || Face;
#else
            return Info != nullptr;
#endif
        }
        bool HasGlyph( uint32_t Cp ) const
        {
#ifdef TG_HAS_FREETYPE
            if ( Face ) return FT_Get_Char_Index( Face, Cp ) != 0;
#endif
            return Info && stbtt_FindGlyphIndex( Info.get(), int( Cp ) ) != 0;
        }
    };

    struct FontAtlas::Packer
    {
        stbrp_context Ctx;
        std::vector<stbrp_node> Nodes;
    };

    static constexpr int Reserved = 4; // top rows: a 2x2 white block for solid shapes

    // A rasterized glyph before packing: bitmap in pixels, offsets in pixels from the pen on the baseline.
    struct GlyphBitmap
    {
        int W = 0, H = 0;
        std::vector<uint32_t> Px; // RGBA (straight alpha)
        float X0 = 0, Y0 = 0, X1 = 0, Y1 = 0, Advance = 0;
        bool Colored = false;
    };

    // ------------------------------------------------------------ rasterizers

    // stb_truetype, with horizontal oversampling (same math as stbtt_PackFontRanges).
    static bool RasterStb( const stbtt_fontinfo* I, int G, float PxHeight, int Oh, int Ov, const uint8_t* Curve, GlyphBitmap& B )
    {
        float Sc = stbtt_ScaleForPixelHeight( I, PxHeight );
        int Adv, Lsb;
        stbtt_GetGlyphHMetrics( I, G, &Adv, &Lsb );
        B.Advance = Adv * Sc;
        int x0, y0, x1, y1;
        stbtt_GetGlyphBitmapBoxSubpixel( I, G, Sc * Oh, Sc * Ov, 0, 0, &x0, &y0, &x1, &y1 );
        if ( x1 <= x0 || y1 <= y0 ) return true; // empty (space)
        B.W = x1 - x0 + Oh - 1;
        B.H = y1 - y0 + Ov - 1;
        std::vector<unsigned char> A( size_t( B.W ) * B.H, 0 );
        float Sx, Sy;
        stbtt_MakeGlyphBitmapSubpixelPrefilter( I, A.data(), B.W, B.H, B.W, Sc * Oh, Sc * Ov, 0, 0, Oh, Ov, &Sx, &Sy, G );
        B.X0 = x0 / float( Oh ) + Sx;
        B.Y0 = y0 / float( Ov ) + Sy;
        B.X1 = ( x0 + B.W ) / float( Oh ) + Sx;
        B.Y1 = ( y0 + B.H ) / float( Ov ) + Sy;
        B.Px.resize( A.size() );
        for ( size_t i = 0; i < A.size(); i++ ) B.Px[ i ] = Color( 255, 255, 255, Curve[ A[ i ] ] ).Pack();
        return true;
    }

#ifdef TG_HAS_FREETYPE
    // Area-average downscale of a premultiplied BGRA bitmap (emoji strikes are ~109-160 px).
    static void Downscale( const unsigned char* Src, int Sw, int Sh, int Pitch, float K, GlyphBitmap& B )
    {
        B.W = std::max( 1, int( std::lround( Sw * K ) ) );
        B.H = std::max( 1, int( std::lround( Sh * K ) ) );
        B.Px.assign( size_t( B.W ) * B.H, 0 );
        for ( int y = 0; y < B.H; y++ )
            for ( int x = 0; x < B.W; x++ )
            {
                int Sx0 = int( x / K ), Sx1 = std::max( Sx0 + 1, std::min( Sw, int( ( x + 1 ) / K ) ) );
                int Sy0 = int( y / K ), Sy1 = std::max( Sy0 + 1, std::min( Sh, int( ( y + 1 ) / K ) ) );
                double Sum[ 4 ] = {};
                int N = 0;
                for ( int sy = Sy0; sy < Sy1 && sy < Sh; sy++ )
                    for ( int sx = Sx0; sx < Sx1 && sx < Sw; sx++, N++ )
                    {
                        const unsigned char* P = Src + size_t( sy ) * Pitch + size_t( sx ) * 4;
                        for ( int c = 0; c < 4; c++ ) Sum[ c ] += P[ c ];
                    }
                if ( !N ) continue;
                double A = Sum[ 3 ] / N;
                if ( A < 0.5 ) continue;
                // premultiplied BGRA -> straight RGBA
                auto Un = [ & ]( double V ) { return uint8_t( std::clamp( V / N * 255.0 / A, 0.0, 255.0 ) ); };
                B.Px[ size_t( y ) * B.W + x ] = Color( Un( Sum[ 2 ] ), Un( Sum[ 1 ] ), Un( Sum[ 0 ] ), uint8_t( A ) ).Pack();
            }
    }

    // FreeType: hinted grayscale text, or color glyphs (CBDT/sbix bitmaps, COLR layers) for emoji.
    static bool RasterFtIndex( FT_Face F, FT_UInt Gi, float EmPx, bool Hinting, const uint8_t* Curve, GlyphBitmap& B )
    {
        if ( !Gi ) return false;
        float K = 1;
        if ( !FT_IS_SCALABLE( F ) && F->num_fixed_sizes > 0 )
        {
            // bitmap-only (Noto Color Emoji): use the strike, scale the bitmap ourselves
            FT_Select_Size( F, 0 );
            K = EmPx / float( F->size->metrics.y_ppem );
        }
        else
            FT_Set_Char_Size( F, 0, FT_F26Dot6( EmPx * 64 ), 72, 72 );

        FT_Int32 Flags = FT_HAS_COLOR( F ) ? FT_LOAD_COLOR : ( Hinting ? FT_LOAD_TARGET_LIGHT : FT_LOAD_NO_HINTING );
        if ( FT_Load_Glyph( F, Gi, Flags ) ) return false;
        FT_GlyphSlot S = F->glyph;
        if ( S->format != FT_GLYPH_FORMAT_BITMAP && FT_Render_Glyph( S, FT_RENDER_MODE_NORMAL ) ) return false;
        const FT_Bitmap& Bm = S->bitmap;
        B.Advance = ( S->advance.x / 64.f ) * K;
        if ( !Bm.width || !Bm.rows ) return true;

        if ( Bm.pixel_mode == FT_PIXEL_MODE_BGRA )
        {
            Downscale( Bm.buffer, int( Bm.width ), int( Bm.rows ), Bm.pitch, K, B );
            B.Colored = true;
        }
        else
        {
            B.W = int( Bm.width );
            B.H = int( Bm.rows );
            B.Px.resize( size_t( B.W ) * B.H );
            for ( int y = 0; y < B.H; y++ )
                for ( int x = 0; x < B.W; x++ )
                {
                    uint8_t A = Bm.pixel_mode == FT_PIXEL_MODE_MONO ? uint8_t( ( ( Bm.buffer[ y * Bm.pitch + x / 8 ] >> ( 7 - x % 8 ) ) & 1 ) * 255 )
                                                                    : Bm.buffer[ y * Bm.pitch + x ];
                    B.Px[ size_t( y ) * B.W + x ] = Color( 255, 255, 255, Curve[ A ] ).Pack();
                }
        }
        B.X0 = S->bitmap_left * K;
        B.Y0 = -S->bitmap_top * K;
        B.X1 = B.X0 + B.W;
        B.Y1 = B.Y0 + B.H;
        return true;
    }

    static bool RasterFt( FT_Face F, uint32_t Cp, float EmPx, bool Hinting, const uint8_t* Curve, GlyphBitmap& B )
    {
        return RasterFtIndex( F, FT_Get_Char_Index( F, Cp ), EmPx, Hinting, Curve, B );
    }
#endif

    // ------------------------------------------------------------ Font

    const Glyph& Font::Find( uint32_t Cp ) const
    {
        auto It = Glyphs.find( Cp );
        if ( It != Glyphs.end() ) return It->second;
        if ( IsZeroWidth( Cp ) ) return Glyphs.emplace( Cp, Glyph{} ).first->second;
        if ( Owner && Owner->Built )
        {
            Glyph G{};
            if ( Owner->LoadGlyph( *this, Cp, G ) ) return Glyphs.emplace( Cp, G ).first->second;
            if ( Owner->WantGrow ) return Fallback; // out of room this frame: retried after the atlas grows
        }
        return Glyphs.emplace( Cp, Fallback ).first->second; // nobody has it: remember, don't retry
    }

    const Glyph* Font::FindSequence( std::string_view Utf8 ) const
    {
        if ( !Owner || !FontAtlas::HasHarfBuzz() ) return nullptr;
        std::string Key( Utf8 );
        auto It = Sequences.find( Key );
        if ( It != Sequences.end() ) return It->second.Index < 0 ? nullptr : &It->second;
        if ( !Owner->Built ) return nullptr;
        Glyph G{};
        if ( Owner->LoadSequence( *this, Utf8, G ) ) return &Sequences.emplace( std::move( Key ), G ).first->second;
        if ( Owner->WantGrow ) return nullptr; // retried after the atlas grows
        G.Index = -1;
        Sequences.emplace( std::move( Key ), G );
        return nullptr;
    }

    const Glyph& Font::Next( std::string_view Str, size_t& i, uint32_t& Cp ) const
    {
        // cheap filter first: sequences start with a digit/#/* (keycaps) or a codepoint >= U+00A9
        unsigned char B0 = (unsigned char)Str[ i ];
        if ( FontAtlas::HasHarfBuzz() && ( B0 >= 0xC2 || ( B0 >= '0' && B0 <= '9' ) || B0 == '#' || B0 == '*' ) )
            if ( size_t L = EmojiSequenceLength( Str, i ) )
                if ( const Glyph* G = FindSequence( Str.substr( i, L ) ) )
                {
                    i += L;
                    Cp = 0;
                    return *G;
                }
        Cp = DecodeUtf8( Str, i );
        return Find( Cp );
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
                uint32_t Cp;
                const Glyph& G = Next( Line, i, Cp );
                X += ( Cp ? Kern( Prev, Cp ) : 0 ) + G.Advance;
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
            uint32_t Cp;
            float Adv = Next( Str, i, Cp ).Advance; // a caret never lands inside an emoji sequence
            if ( Cp ) Pen += Kern( Prev, Cp );
            Prev = Cp;
            if ( X < Pen + Adv * 0.5f ) return Start;
            Pen += Adv;
        }
        return Str.size();
    }

    // ------------------------------------------------------------ FontAtlas: sources

    FontAtlas::FontAtlas() = default;
    FontAtlas::~FontAtlas() = default;

    bool FontAtlas::HasHarfBuzz()
    {
#ifdef TG_HAS_HARFBUZZ
        return true;
#else
        return false;
#endif
    }

    bool FontAtlas::HasFreeType()
    {
#ifdef TG_HAS_FREETYPE
        return FtLib() != nullptr;
#else
        return false;
#endif
    }

    static bool ReadFile( const std::string& Path, std::vector<unsigned char>& Out )
    {
        FILE* Fp = std::fopen( Path.c_str(), "rb" );
        if ( !Fp ) return false;
        std::fseek( Fp, 0, SEEK_END );
        long Len = std::ftell( Fp );
        std::fseek( Fp, 0, SEEK_SET );
        Out.resize( size_t( Len > 0 ? Len : 0 ) );
        size_t Got = std::fread( Out.data(), 1, Out.size(), Fp );
        std::fclose( Fp );
        return Got == Out.size() && !Out.empty();
    }

    Font* FontAtlas::AddFromMemory( std::vector<unsigned char> Data, float PixelSize )
    {
        auto S = std::make_unique<Source>();
        S->Data = std::make_shared<std::vector<unsigned char>>( std::move( Data ) );
        S->Size = PixelSize;
        if ( !S->Open() ) return nullptr;
        Fonts.push_back( std::make_unique<Font>() );
        Font* F = Fonts.back().get();
        F->Size = PixelSize;
        F->Owner = this;
        F->Source = int( Sources.size() );
        S->Out = F;
        Sources.push_back( std::move( S ) );
        return F;
    }

    Font* FontAtlas::AddFromFile( const std::string& Path, float PixelSize )
    {
        std::vector<unsigned char> D;
        return ReadFile( Path, D ) ? AddFromMemory( std::move( D ), PixelSize ) : nullptr;
    }

    bool FontAtlas::AddFallbackMemory( std::vector<unsigned char> Data )
    {
        auto S = std::make_unique<Source>();
        S->Data = std::make_shared<std::vector<unsigned char>>( std::move( Data ) );
        if ( !S->Open() ) return false;
        Fallbacks.push_back( std::move( S ) );
        return true;
    }

    bool FontAtlas::AddFallbackFile( const std::string& Path )
    {
        std::vector<unsigned char> D;
        return ReadFile( Path, D ) && AddFallbackMemory( std::move( D ) );
    }

    bool FontAtlas::HasColorFallback() const
    {
        for ( auto& F : Fallbacks )
            if ( F->Color ) return true;
        return false;
    }

    bool FontAtlas::AddEmojiFont()
    {
        if ( !HasFreeType() ) return false;
        if ( HasColorFallback() ) return true;
        const char* Paths[] = {
            "C:/Windows/Fonts/seguiemj.ttf",                         // Windows 10/11
            "/System/Library/Fonts/Apple Color Emoji.ttc",           // macOS
            "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",     // Debian / Ubuntu
            "/usr/share/fonts/noto-emoji/NotoColorEmoji.ttf",        // Arch
            "/usr/share/fonts/google-noto-emoji/NotoColorEmoji.ttf", // Fedora
            "/usr/share/fonts/noto/NotoColorEmoji.ttf",
            "/usr/local/share/fonts/NotoColorEmoji.ttf",
        };
        for ( const char* P : Paths )
        {
            if ( !AddFallbackFile( P ) ) continue;
#ifdef TG_HAS_FREETYPE
            // a bitmap (PNG) emoji font needs FreeType built with libpng; if this one can't draw, try the next
            FT_Face F = Fallbacks.back()->Face;
            bool Ok = F && FT_Get_Char_Index( F, 0x1F600 );
            if ( Ok && !FT_IS_SCALABLE( F ) && F->num_fixed_sizes > 0 ) FT_Select_Size( F, 0 );
            else if ( Ok ) FT_Set_Char_Size( F, 0, 32 * 64, 72, 72 );
            if ( Ok && FT_Load_Glyph( F, FT_Get_Char_Index( F, 0x1F600 ), FT_LOAD_COLOR ) == 0 ) return true;
            Fallbacks.pop_back();
#else
            return true;
#endif
        }
        return false;
    }

    bool FontAtlas::AddCjkFont()
    {
        const char* Paths[] = {
            "C:/Windows/Fonts/msyh.ttc",                                  // Windows: Microsoft YaHei
            "C:/Windows/Fonts/YuGothM.ttc",
            "C:/Windows/Fonts/malgun.ttf",
            "/System/Library/Fonts/PingFang.ttc",                         // macOS
            "/System/Library/Fonts/Hiragino Sans GB.ttc",
            "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",     // Debian / Ubuntu
            "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",          // Arch
            "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",   // Fedora
            "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
            "/usr/share/fonts/wenquanyi/wqy-zenhei/wqy-zenhei.ttc",
        };
        for ( const char* P : Paths )
            if ( FILE* F = std::fopen( P, "rb" ) )
            {
                std::fclose( F );
                auto S = std::make_unique<Source>();
                S->LazyPath = P; // nothing is read until a glyph actually needs it
                Fallbacks.push_back( std::move( S ) );
                return true;
            }
        return false;
    }

    // ------------------------------------------------------------ FontAtlas: packing

    bool FontAtlas::Grow( int NewH )
    {
        if ( NewH <= Height || NewH > 8192 + Reserved ) return false;
        int OldH = Height;
        Pixels.resize( size_t( Width ) * NewH, 0 );
        Height = NewH;
        Pack->Ctx.height = NewH - Reserved; // the skyline packer just gets more room at the bottom
        float K = float( OldH ) / NewH;     // existing glyphs keep their texels; their normalized V shrinks
        for ( auto& F : Fonts )
        {
            for ( auto& [ Cp, G ] : F->Glyphs ) { G.V0 *= K; G.V1 *= K; }
            for ( auto& [ Seq, G ] : F->Sequences ) { G.V0 *= K; G.V1 *= K; }
            F->Fallback.V0 *= K;
            F->Fallback.V1 *= K;
        }
        WhiteUV = { 2.f / Width, 2.f / Height };
        return true;
    }

    // Rasterize Cp for font F (from F's own source, else from a fallback), pack it, write the pixels.
    bool FontAtlas::LoadGlyph( const Font& F, uint32_t Cp, Glyph& Out )
    {
        Source& Own = *Sources[ size_t( F.Source ) ];
        const float PxHeight = F.Size * Scale;
        const int Oh = std::clamp( Quality.OversampleH, 1, 8 ), Ov = std::clamp( Quality.OversampleV, 1, 8 );
        GlyphBitmap B;
        bool FromOwn = false;

        auto TrySource = [ & ]( Source& S ) -> bool {
            if ( !S.HasGlyph( Cp ) ) return false;
#ifdef TG_HAS_FREETYPE
            if ( S.Face && ( S.Color || !S.Info || Quality.Engine == FontEngine::FreeType ) )
            {
                float EmPx = PxHeight;
                if ( S.Color ) EmPx = PxHeight * 0.82f * Quality.EmojiScale;                                     // emoji ~ text height
                else if ( S.Info ) EmPx = stbtt_ScaleForPixelHeight( S.Info.get(), PxHeight ) * S.Face->units_per_EM; // same scale as stb
                if ( !RasterFt( S.Face, Cp, EmPx, Quality.Hinting, Curve, B ) ) return false;
                if ( S.Info && !B.Colored ) // keep layout identical between engines: unhinted advance
                {
                    int Adv, Lsb;
                    stbtt_GetGlyphHMetrics( S.Info.get(), stbtt_FindGlyphIndex( S.Info.get(), int( Cp ) ), &Adv, &Lsb );
                    B.Advance = Adv * stbtt_ScaleForPixelHeight( S.Info.get(), PxHeight );
                }
                return true;
            }
#endif
            if ( !S.Info ) return false;
            return RasterStb( S.Info.get(), stbtt_FindGlyphIndex( S.Info.get(), int( Cp ) ), PxHeight, Oh, Ov, Curve, B );
        };

        bool Got = false;
        if ( TrySource( Own ) ) Got = FromOwn = true;
        else
            for ( auto& Fb : Fallbacks )
                if ( Ready( *Fb ) && TrySource( *Fb ) ) { Got = true; break; }
        if ( !Got ) return false;
        if ( !PackBitmap( B, Out ) ) return false;
        Out.Index = FromOwn && Own.Info ? stbtt_FindGlyphIndex( Own.Info.get(), int( Cp ) ) : 0;
        return true;
    }

    FontAtlas::Source* FontAtlas::Ready( Source& S )
    {
        if ( !S.LazyPath.empty() )
        {
            std::string P = std::move( S.LazyPath );
            S.LazyPath.clear();
            auto D = std::make_shared<std::vector<unsigned char>>();
            if ( !ReadFile( P, *D ) ) return nullptr;
            S.Data = std::move( D );
            S.Open();
        }
        return S.Data ? &S : nullptr;
    }

    // Shape an emoji sequence with the color fallback fonts; succeed only if it becomes one glyph.
    bool FontAtlas::LoadSequence( const Font& F, std::string_view Utf8, Glyph& Out )
    {
#if defined( TG_HAS_HARFBUZZ ) && defined( TG_HAS_FREETYPE )
        const float PxHeight = F.Size * Scale;
        for ( auto& Fb : Fallbacks )
        {
            if ( !Ready( *Fb ) || !Fb->Color || !Fb->Face ) continue;
            hb_font_t* Hf = Fb->Shaper();
            if ( !Hf ) continue;
            hb_buffer_t* Buf = hb_buffer_create();
            hb_buffer_add_utf8( Buf, Utf8.data(), int( Utf8.size() ), 0, int( Utf8.size() ) );
            hb_buffer_guess_segment_properties( Buf );
            hb_shape( Hf, Buf, nullptr, 0 );
            unsigned N = 0;
            hb_glyph_info_t* Info = hb_buffer_get_glyph_infos( Buf, &N );
            hb_glyph_position_t* Pos = hb_buffer_get_glyph_positions( Buf, &N );
            // one visible glyph (leftover joiners/selectors may remain as zero-advance glyphs)
            unsigned Gid = 0, Visible = 0;
            bool Missing = false;
            for ( unsigned k = 0; k < N; k++ )
            {
                if ( Info[ k ].codepoint == 0 ) Missing = true;
                else if ( Pos[ k ].x_advance != 0 ) { Gid = Info[ k ].codepoint; Visible++; }
            }
            hb_buffer_destroy( Buf );
            if ( Missing || Visible != 1 ) continue;
            GlyphBitmap B;
            if ( !RasterFtIndex( Fb->Face, Gid, PxHeight * 0.82f * Quality.EmojiScale, Quality.Hinting, Curve, B ) ) continue;
            if ( !PackBitmap( B, Out ) ) return false;
            Out.Index = 0;
            return true;
        }
#else
        (void)F;
        (void)Utf8;
        (void)Out;
#endif
        return false;
    }

    // Pack a rasterized glyph into the atlas and fill in its quad + UVs.
    bool FontAtlas::PackBitmap( const GlyphBitmap& B, Glyph& Out )
    {

        // pack (1px padding around every glyph so bilinear sampling never bleeds into a neighbour)
        stbrp_rect R{};
        if ( B.W && B.H )
        {
            R.w = B.W + 2;
            R.h = B.H + 2;
            stbrp_pack_rects( &Pack->Ctx, &R, 1 );
            if ( !R.was_packed )
            {
                WantGrow = true; // deferred: growing changes every glyph's UVs, not safe mid-frame
                if ( RequestFrame ) RequestFrame();
                return false;
            }
            for ( int y = 0; y < B.H; y++ )
                std::memcpy( &Pixels[ size_t( R.y + Reserved + 1 + y ) * Width + R.x + 1 ], &B.Px[ size_t( y ) * B.W ], size_t( B.W ) * 4 );
            Dirty = true;
        }

        Glyph& G = Out;
        G.X0 = B.X0 / Scale; G.Y0 = B.Y0 / Scale;
        G.X1 = B.X1 / Scale; G.Y1 = B.Y1 / Scale;
        G.U0 = ( R.x + 1 ) / float( Width );
        G.V0 = ( R.y + Reserved + 1 ) / float( Height );
        G.U1 = ( R.x + 1 + B.W ) / float( Width );
        G.V1 = ( R.y + Reserved + 1 + B.H ) / float( Height );
        G.Advance = B.Advance / Scale;
        G.Colored = B.Colored;
        Loaded++;
        return true;
    }

    // Baked up front; everything else (emoji, CJK, symbols, box drawing...) loads on first use.
    static void CollectCodepoints( std::vector<uint32_t>& Out )
    {
        auto Range = [ & ]( uint32_t A, uint32_t B ) { for ( uint32_t c = A; c <= B; c++ ) Out.push_back( c ); };
        Range( 0x20, 0x7E );     // ASCII
        Range( 0xA0, 0xFF );     // Latin-1
        Range( 0x2010, 0x2044 ); // dashes, quotes, bullet, ellipsis
    }

    bool FontAtlas::Build( Renderer& R, float S )
    {
        Scale = S;
        for ( int a = 0; a < 256; a++ )
            Curve[ a ] = uint8_t( std::lround( 255.0 * std::pow( a / 255.0, 1.0 / std::max( 0.1f, Quality.Gamma ) ) ) );

        Width = 1024;
        Height = 512 + Reserved;
        Pixels.assign( size_t( Width ) * Height, 0 );
        for ( int y = 1; y <= 2; y++ )
            for ( int x = 1; x <= 2; x++ ) Pixels[ size_t( y ) * Width + x ] = 0xFFFFFFFFu;
        WhiteUV = { 2.f / Width, 2.f / Height };
        Pack = std::make_unique<Packer>();
        Pack->Nodes.resize( size_t( Width ) );
        stbrp_init_target( &Pack->Ctx, Width, Height - Reserved, Pack->Nodes.data(), int( Pack->Nodes.size() ) );
        Built = true;
        Loaded = 0;
        WantGrow = false;

        const int Oh = std::clamp( Quality.OversampleH, 1, 8 );
        for ( auto& Sp : Sources )
        {
            Source& Src = *Sp;
            Font* F = Src.Out;
            F->Glyphs.clear();
            F->KernCache.clear();
            F->Fallback = Glyph{};
            bool Ft = false;
#ifdef TG_HAS_FREETYPE
            Ft = Quality.Engine == FontEngine::FreeType && Src.Face;
#endif
            F->SubpixelSteps = Ft ? 1.f : float( Oh ); // hinted glyphs sit on whole pixels
            if ( Src.Info )
            {
                float Px = stbtt_ScaleForPixelHeight( Src.Info.get(), Src.Size * Scale );
                int Asc, Desc, Gap;
                stbtt_GetFontVMetrics( Src.Info.get(), &Asc, &Desc, &Gap );
                F->Ascent = std::round( Asc * Px ) / Scale;
                F->LineHeight = std::round( ( Asc - Desc + Gap ) * Px ) / Scale;
                F->Info = Src.Info;
                F->KernScale = Px / Scale;
                F->Kerning = Quality.Kerning;
            }
            else
            {
                F->Ascent = std::round( Src.Size * 0.8f );
                F->LineHeight = std::round( Src.Size * 1.2f );
            }
        }

        // bake the base set; grow right away if it doesn't fit (nothing references UVs yet)
        std::vector<uint32_t> Cps;
        CollectCodepoints( Cps );
        for ( auto& Sp : Sources )
        {
            Font* F = Sp->Out;
            for ( uint32_t Cp : Cps )
            {
                Glyph G{};
                bool Ok = LoadGlyph( *F, Cp, G );
                while ( !Ok && WantGrow )
                {
                    WantGrow = false;
                    if ( !Grow( ( Height - Reserved ) * 2 + Reserved ) ) break;
                    Ok = LoadGlyph( *F, Cp, G );
                }
                if ( Ok ) F->Glyphs[ Cp ] = G;
            }
            auto Q = F->Glyphs.find( '?' );
            if ( Q != F->Glyphs.end() ) F->Fallback = Q->second;
        }

        if ( Texture ) R.DestroyTexture( Texture );
        Texture = R.CreateTexture( Width, Height, Pixels.data() );
        Dirty = false;
        return true;
    }

    void FontAtlas::Maintain( Renderer& R )
    {
        if ( !WantGrow || !Built ) return;
        WantGrow = false;
        if ( !Grow( ( Height - Reserved ) * 2 + Reserved ) ) return;
        if ( Texture ) R.DestroyTexture( Texture );
        Texture = R.CreateTexture( Width, Height, Pixels.data() );
        Dirty = false;
    }

    void FontAtlas::Upload( Renderer& R )
    {
        if ( !Dirty || !Texture ) return;
        R.UpdateTexture( Texture, Width, Height, Pixels.data() );
        Dirty = false;
    }
}
