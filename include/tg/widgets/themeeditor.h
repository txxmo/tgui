// ThemeEditor — edit the theme live: a color picker per token, the corner radius, presets, export.
//
//   auto& Win = App.Window( "Theme", {}, { 360, 560 } );
//   Win.Add<tg::ThemeEditor>( "theme.json" );   // Save / Load use this file (optional)
#pragma once
#include "container.h"
#include <string>

namespace tg
{
    class ThemeEditor : public Container
    {
    public:
        explicit ThemeEditor( std::string Path = {} );
        std::string Path;
        std::string CppSnippet() const; // the theme as C++ ("T[ Tok::Accent ] = Color::Hex( 0x6EB7D4 );" ...)

    private:
        void Changed();
    };
}
