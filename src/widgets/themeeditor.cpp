#include "tg/widgets/themeeditor.h"
#include "tg/context.h"
#include "tg/theme.h"
#include "tg/widgets/button.h"
#include "tg/widgets/colorpicker.h"
#include "tg/widgets/label.h"
#include "tg/widgets/slider.h"
#include "tg/widgets/toast.h"
#include <cstdio>

namespace tg
{
    ThemeEditor::ThemeEditor( std::string P ) : Container( Dir::Column, 6 ), Path( std::move( P ) )
    {
        Layout.Width = Grow();
        LabelWidth = 120;
        auto& Presets = Row( 6 );
        Presets.Button( "Dark", [ this ] { GetTheme().UseColors( Theme::Dark() ); Changed(); } );
        Presets.Button( "Light", [ this ] { GetTheme().UseColors( Theme::Light() ); Changed(); } );
        Theme& T = GetTheme();
        for ( size_t i = 0; i < size_t( Tok::Count ); i++ )
        {
            auto& Cp = ColorPicker( Theme::TokenName( Tok( i ) ), &T.Colors[ i ] );
            Cp.OnChange = [ this ]( tg::Color ) { Changed(); };
        }
        auto& R = Slider( "Radius", &T.Radius, 0, 16, 1, "%.0f" );
        auto Show = R.OnChange;
        R.OnChange = [ this, Show ]( float V ) { if ( Show ) Show( V ); Changed(); };
        auto& Out = Row( 6 );
        Out.Button( "Copy JSON", [ this ] { if ( Context* C = GetContext(); C && C->SetClipboard ) { C->SetClipboard( GetTheme().ToJson() ); Toast( *C, "Theme JSON copied" ); } } );
        Out.Button( "Copy C++", [ this ] { if ( Context* C = GetContext(); C && C->SetClipboard ) { C->SetClipboard( CppSnippet() ); Toast( *C, "C++ copied" ); } } );
        if ( !Path.empty() )
        {
            Out.Button( "Save", [ this ] { if ( Context* C = GetContext() ) Toast( *C, SaveThemeFile( Path ) ? "Saved " + Path : "Couldn't write " + Path ); } );
            Out.Button( "Load", [ this ] { if ( LoadThemeFile( Path ) ) Changed(); } );
        }
    }

    void ThemeEditor::Changed()
    {
        if ( Context* C = GetContext() ) C->InvalidateLayout(); // every widget reads the theme as it draws
    }

    std::string ThemeEditor::CppSnippet() const
    {
        std::string S = "tg::Theme& T = tg::GetTheme();\n";
        char B[ 96 ];
        const Theme& T = GetTheme();
        for ( size_t i = 0; i < size_t( Tok::Count ); i++ )
        {
            tg::Color C = T.Colors[ i ];
            if ( C.a == 255 ) std::snprintf( B, sizeof( B ), "T[ tg::Tok::%s ] = tg::Color::Hex( 0x%02X%02X%02X );\n", Theme::TokenName( Tok( i ) ), C.r, C.g, C.b );
            else std::snprintf( B, sizeof( B ), "T[ tg::Tok::%s ] = tg::Color( %d, %d, %d, %d );\n", Theme::TokenName( Tok( i ) ), C.r, C.g, C.b, C.a );
            S += B;
        }
        std::snprintf( B, sizeof( B ), "T.Radius = %g;\n", T.Radius );
        return S + B;
    }
}
