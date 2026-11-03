// tg/font.h — TTF/OTF fonts baked into one RGBA atlas (via stb_truetype).
#pragma once
#include "core.h"
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tg
{
    class Renderer;

    struct Glyph
    {
        float X0, Y0, X1, Y1; // quad relative to pen position on the baseline
        float U0, V0, U1, V1;
        float Advance;
    };

    class Font
    {
    public:
        float Size = 0;       // pixel height requested
        float Ascent = 0;     // baseline offset from the top of a line
        float LineHeight = 0; // distance between baselines

        const Glyph& Find( uint32_t Codepoint ) const;

        // Size of the text block. WrapWidth > 0 wraps on spaces like DrawList::Text does.
        Vec2 Measure( std::string_view Str, float WrapWidth = 0 ) const;
        float Width( std::string_view Str ) const { return Measure( Str ).x; }

        // Byte offset in Str whose caret position is closest to X (for text inputs).
        size_t HitTest( std::string_view Str, float X ) const;

        // Walk wrapped lines: Fn( std::string_view Line, float Y ). Shared by Measure & Text so they never disagree.
        template<typename F> void ForEachLine( std::string_view Str, float WrapWidth, F&& Fn ) const;

    private:
        friend class FontAtlas;
        std::unordered_map<uint32_t, Glyph> Glyphs;
        Glyph Fallback{};
    };

    class FontAtlas
    {
    public:
        FontAtlas();
        ~FontAtlas();

        // Queue fonts. They are rasterized when Build() runs. Returns nullptr if the file can't be read.
        Font* AddFromFile( const std::string& Path, float PixelSize );
        Font* AddFromMemory( std::vector<unsigned char> Data, float PixelSize );

        // Rasterize everything into Pixels and upload to the renderer.
        // Scale = DPI scale: glyphs are rasterized at Size*Scale pixels but measured in UI units.
        bool Build( Renderer& R, float Scale = 1.f );

        int Width = 0, Height = 0;
        std::vector<uint32_t> Pixels; // RGBA, white with glyph coverage in alpha
        TextureId Texture = 0;
        Vec2 WhiteUV;

    private:
        struct Source { std::shared_ptr<std::vector<unsigned char>> Data; float Size; Font* Out; };
        std::vector<Source> Sources;
        std::vector<std::unique_ptr<Font>> Fonts;
    };

    // ---------- inline: line walking ----------
    uint32_t DecodeUtf8( std::string_view s, size_t& i );

    template<typename F>
    void Font::ForEachLine( std::string_view Str, float WrapWidth, F&& Fn ) const
    {
        float Y = 0;
        size_t LineStart = 0;
        while ( LineStart <= Str.size() )
        {
            size_t i = LineStart, LastBreak = std::string_view::npos;
            float X = 0;
            size_t LineEnd = Str.size();
            size_t Next = Str.size() + 1;
            while ( i < Str.size() )
            {
                size_t Prev = i;
                uint32_t Cp = DecodeUtf8( Str, i );
                if ( Cp == '\n' ) { LineEnd = Prev; Next = i; break; }
                if ( Cp == ' ' ) LastBreak = Prev;
                X += Find( Cp ).Advance;
                if ( WrapWidth > 0 && X > WrapWidth && Prev > LineStart )
                {
                    if ( LastBreak != std::string_view::npos ) { LineEnd = LastBreak; Next = LastBreak + 1; }
                    else { LineEnd = Prev; Next = Prev; } // no space: hard break mid-word
                    break;
                }
            }
            Fn( Str.substr( LineStart, LineEnd - LineStart ), Y );
            Y += LineHeight;
            if ( Next > Str.size() ) break;
            LineStart = Next;
        }
    }
}
