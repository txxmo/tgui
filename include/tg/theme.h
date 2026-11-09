// tg/theme.h — the palette. Change a color here and every widget follows.
//
// Styling has three levels, most global first:
//   1. Theme       — named colors + fonts + base radius.         tg::GetTheme().Colors[ Tok::Accent ] = ...
//   2. Type style  — per-widget-type defaults (Checkbox::Defaults()). Every *new* checkbox copies it.
//   3. Instance    — each widget's own `Style` member.            MyBox.Style.Radius = 0;
// And if that isn't enough: the widget's Draw() is ~30 lines in src/widgets/<name>.cpp. Edit it, or subclass.
#pragma once
#include "core.h"
#include <string>

namespace tg
{
    class Font;

    // Theme color slots ("tokens").
    enum class Tok : uint8_t
    {
        Background, // window/app background
        Surface,    // panels, cards
        SurfaceAlt, // inputs, wells, hovered rows
        Raised,     // buttons, slider tracks
        Border,
        BorderStrong,
        Text,
        TextDim,
        TextOnAccent,
        Accent,
        AccentHover,
        Success,
        Warning,
        Danger,
        Count
    };

    // A color that is either a theme token or a literal. Style structs use this so they
    // follow theme changes by default but can still be pinned to an exact color.
    struct Paint
    {
        constexpr Paint( Tok T ) : IsToken( true ), Token( T ) {}
        constexpr Paint( Color C ) : IsToken( false ), Literal( C ) {}
        bool IsToken;
        Tok Token = Tok::Text;
        Color Literal;
        Color Resolve() const;
    };

    struct Theme
    {
        Color Colors[ size_t( Tok::Count ) ];
        float Radius = 6.f;
        Font* Body = nullptr;  // default text
        Font* Medium = nullptr; // between Body and Bold: emphasis that shouldn't shout
        Font* Bold = nullptr;  // headings / emphasis
        Font* Small = nullptr; // captions, axis labels
        Font* Title = nullptr; // big headings
        Font* Italic = nullptr;   // italic body text (RichText *emphasis*); nullptr = Medium
        Font* Mono = nullptr;     // monospace: terminals, code, logs (JetBrains Mono)
        Font* MonoBold = nullptr;

        Color& operator[]( Tok t ) { return Colors[ size_t( t ) ]; }
        Color operator[]( Tok t ) const { return Colors[ size_t( t ) ]; }

        static Theme Dark();
        static Theme Light();

        // Theme files (JSON): { "Accent": "#6EB7D4", "Background": "#0E1116", ..., "Radius": 6 }.
        // Missing keys keep their value, so a file can change just a few colors.
        std::string ToJson() const;
        bool FromJson( const std::string& Text );
        static const char* TokenName( Tok T ); // "Accent"

        // Swap palettes at runtime without losing fonts:  GetTheme().UseColors( Theme::Light() );
        void UseColors( const Theme& Other )
        {
            for ( size_t i = 0; i < size_t( Tok::Count ); i++ ) Colors[ i ] = Other.Colors[ i ];
        }
    };

    // The active theme. Plain global: read it in Draw(), poke it from your app.
    Theme& GetTheme();
    bool LoadThemeFile( const std::string& Path ); // into GetTheme(); Ctx.WatchTheme( Path ) reloads on change
    bool SaveThemeFile( const std::string& Path );
    std::string ThemeFileStamp( const std::string& Path ); // modification time + size ("" if missing)

    inline Color Paint::Resolve() const { return IsToken ? GetTheme()[ Token ] : Literal; }
}
