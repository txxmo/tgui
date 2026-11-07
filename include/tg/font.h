// tg/font.h — TTF/OTF fonts in one RGBA atlas.
//
// Rasterizer: stb_truetype by default (no dependencies), or FreeType when tg is built with
// TG_FREETYPE=ON (hinting; also required for color emoji). Glyphs that aren't baked up front are
// rasterized on demand the first time they're drawn — from the font itself or from fallback fonts
// (emoji, CJK...) — and packed into the atlas, which grows as needed.
//
// With TG_HARFBUZZ=ON, emoji sequences (flags 🇯🇵, ZWJ families 👨‍👩‍👧, skin tones 👋🏽, keycaps 1️⃣,
// tag flags 🏴󠁧󠁢󠁳󠁣󠁴󠁿) are shaped with HarfBuzz and drawn as the single glyph the emoji font has for them.
#pragma once
#include "core.h"
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tg
{
    class Renderer;
    class FontAtlas;
    struct GlyphBitmap; // internal: a rasterized glyph before packing

    struct Glyph
    {
        float X0, Y0, X1, Y1; // quad relative to pen position on the baseline
        float U0, V0, U1, V1;
        float Advance;
        int Index = 0;        // glyph id in the font (for kerning); 0 for glyphs from fallback fonts
        bool Colored = false; // full-color glyph (emoji): drawn untinted, only faded by the text alpha
    };

    enum class FontEngine : uint8_t { Stb, FreeType };

    // How glyphs are rasterized. Defaults are tuned for UI text; change before Build().
    struct FontQuality
    {
        int OversampleH = 2;   // stb: horizontal oversampling, glyphs sit on 1/OversampleH pixel -> even spacing
        int OversampleV = 1;
        float Gamma = 1.3f;    // >1 boosts partial coverage: crisper, less "washed out" strokes (1 = linear)
        bool Kerning = true;   // apply the font's pair kerning (AV, To, ...)
        FontEngine Engine = FontEngine::Stb; // FreeType needs TG_FREETYPE=ON (FontAtlas::HasFreeType())
        bool Hinting = true;   // FreeType: light (vertical-only) hinting = sharper text, same layout
        float EmojiScale = 1.0f; // emoji size relative to the text size
    };

    class Font
    {
    public:
        float Size = 0;       // pixel height requested
        float Ascent = 0;     // baseline offset from the top of a line
        float LineHeight = 0; // distance between baselines

        // Looks the glyph up, rasterizing it (or pulling it from a fallback font) on first use.
        const Glyph& Find( uint32_t Codepoint ) const;
        // The glyph an emoji sequence (UTF-8, see EmojiSequenceLength) shapes to, or nullptr when there's no
        // shaper or the font has no single glyph for it (then draw its codepoints one by one).
        const Glyph* FindSequence( std::string_view Utf8 ) const;
        // Next glyph of Str at byte i: a whole emoji sequence when it shapes to one glyph, else one codepoint.
        // Advances i. Cp = the codepoint, or 0 for a sequence (no kerning around it).
        const Glyph& Next( std::string_view Str, size_t& i, uint32_t& Cp ) const;
        // Extra advance between two codepoints from the font's kerning (0 if none / disabled).
        float Kern( uint32_t A, uint32_t B ) const;
        float SubpixelSteps = 1;   // glyphs are positioned on 1/SubpixelSteps of a pixel

        // Size of the text block. WrapWidth > 0 wraps on spaces like DrawList::Text does.
        Vec2 Measure( std::string_view Str, float WrapWidth = 0 ) const;
        float Width( std::string_view Str ) const { return Measure( Str ).x; }

        // Byte offset in Str whose caret position is closest to X (for text inputs).
        size_t HitTest( std::string_view Str, float X ) const;

        // Walk wrapped lines: Fn( std::string_view Line, float Y ). Shared by Measure & Text so they never disagree.
        template<typename F> void ForEachLine( std::string_view Str, float WrapWidth, F&& Fn ) const;

    private:
        friend class FontAtlas;
        FontAtlas* Owner = nullptr;
        int Source = -1;
        mutable std::unordered_map<uint32_t, Glyph> Glyphs;
        mutable std::unordered_map<std::string, Glyph> Sequences; // Index = -1: no single glyph
        Glyph Fallback{};
        std::shared_ptr<void> Info;   // stbtt_fontinfo (+ the font bytes it points into), for kerning
        float KernScale = 0;          // font units -> UI units
        bool Kerning = false;
        mutable std::unordered_map<uint64_t, float> KernCache;
    };

    class FontAtlas
    {
    public:
        FontAtlas();
        ~FontAtlas();

        // Queue fonts. They are rasterized when Build() runs. Returns nullptr if the file can't be read.
        Font* AddFromFile( const std::string& Path, float PixelSize );
        Font* AddFromMemory( std::vector<unsigned char> Data, float PixelSize );

        // Fallback fonts: searched, in order, for any codepoint a font doesn't have (emoji, CJK, symbols).
        // Color fonts (emoji) need FreeType; without it they're ignored.
        bool AddFallbackFile( const std::string& Path );
        bool AddFallbackMemory( std::vector<unsigned char> Data );
        // Finds the system color emoji font (Segoe UI Emoji / Noto Color Emoji / Apple Color Emoji). Needs FreeType.
        bool AddEmojiFont();
        // Finds a system CJK font (YaHei / PingFang / Noto Sans CJK / WenQuanYi). Read lazily: costs nothing
        // until a Chinese/Japanese/Korean character is actually drawn. Works with or without FreeType.
        bool AddCjkFont();
        static bool HasFreeType();
        static bool HasHarfBuzz(); // built with TG_HARFBUZZ: emoji sequences shape to single glyphs

        // Rasterize the base glyph set into Pixels and upload. Scale = DPI scale: glyphs are rasterized
        // at Size*Scale pixels but measured in UI units. Calling it again (DPI change) rebuilds everything.
        bool Build( Renderer& R, float Scale = 1.f );

        // Called by Context: grow the atlas if on-demand glyphs ran out of room (start of a frame),
        // and push new glyphs to the GPU (before rendering).
        void Maintain( Renderer& R );
        void Upload( Renderer& R );

        FontQuality Quality;
        std::function<void()> RequestFrame; // set by Context: asks for another frame when the atlas must grow
        int Width = 0, Height = 0;
        std::vector<uint32_t> Pixels; // RGBA: white + coverage alpha for text, real colors for emoji
        TextureId Texture = 0;
        Vec2 WhiteUV;
        size_t GlyphCount() const { return Loaded; }
        bool HasColorFallback() const;

    private:
        friend class Font;
        struct Source;
        struct Packer;
        std::vector<std::unique_ptr<Source>> Sources;   // primary fonts, in AddFrom* order
        std::vector<std::unique_ptr<Source>> Fallbacks;
        std::vector<std::unique_ptr<Font>> Fonts;
        std::unique_ptr<Packer> Pack;
        float Scale = 1;
        bool Dirty = false, WantGrow = false, Built = false;
        size_t Loaded = 0;
        uint8_t Curve[ 256 ] = {};

        bool LoadGlyph( const Font& F, uint32_t Codepoint, Glyph& Out ); // rasterize + pack
        bool LoadSequence( const Font& F, std::string_view Utf8, Glyph& Out ); // shape (HarfBuzz) + rasterize + pack
        bool PackBitmap( const GlyphBitmap& B, Glyph& Out );
        Source* Ready( Source& S ); // reads a lazily-added fallback on first use
        bool Grow( int NewHeight );
    };

    // ---------- inline: line walking ----------
    uint32_t DecodeUtf8( std::string_view s, size_t& i );
    // Byte length of the emoji sequence starting at s[i] (regional-indicator pair, keycap, emoji with
    // VS16 / skin tone / tags, ZWJ chains), or 0 if no multi-codepoint sequence starts there.
    size_t EmojiSequenceLength( std::string_view s, size_t i );
    // Caret steps that keep emoji sequences and trailing modifiers together (text fields: arrows, Backspace).
    size_t NextGrapheme( std::string_view s, size_t i );
    size_t PrevGrapheme( std::string_view s, size_t i );

    template<typename F>
    void Font::ForEachLine( std::string_view Str, float WrapWidth, F&& Fn ) const
    {
        float Y = 0;
        size_t LineStart = 0;
        while ( LineStart <= Str.size() )
        {
            size_t i = LineStart, LastBreak = std::string_view::npos;
            float X = 0;
            uint32_t PrevCp = 0;
            size_t LineEnd = Str.size();
            size_t Next = Str.size() + 1;
            while ( i < Str.size() )
            {
                size_t Prev = i;
                uint32_t Cp;
                const Glyph& G = this->Next( Str, i, Cp );
                if ( Cp == '\n' ) { LineEnd = Prev; Next = i; break; }
                if ( Cp == ' ' ) LastBreak = Prev;
                X += ( Cp ? Kern( PrevCp, Cp ) : 0 ) + G.Advance;
                PrevCp = Cp;
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
