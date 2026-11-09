#include "tg/theme.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace tg
{
    Theme Theme::Dark()
    {
        Theme T;
        T[ Tok::Background ]   = Color::Hex( 0x0E1014 );
        T[ Tok::Surface ]      = Color::Hex( 0x161920 );
        T[ Tok::SurfaceAlt ]   = Color::Hex( 0x1D212A );
        T[ Tok::Raised ]       = Color::Hex( 0x262B36 );
        T[ Tok::Border ]       = Color::Hex( 0x262A33 );
        T[ Tok::BorderStrong ] = Color::Hex( 0x3A4150 );
        T[ Tok::Text ]         = Color::Hex( 0xE6E8EE );
        T[ Tok::TextDim ]      = Color::Hex( 0x8B93A7 );
        T[ Tok::TextOnAccent ] = Color::Hex( 0x0B1620 );
        T[ Tok::Accent ]       = Color::Hex( 0x6EB7D4 ); // the old tgui accent
        T[ Tok::AccentHover ]  = Color::Hex( 0x8DCAE2 );
        T[ Tok::Success ]      = Color::Hex( 0x3FB97A );
        T[ Tok::Warning ]      = Color::Hex( 0xE3A33B );
        T[ Tok::Danger ]       = Color::Hex( 0xE5534B );
        return T;
    }

    Theme Theme::Light()
    {
        Theme T;
        T[ Tok::Background ]   = Color::Hex( 0xF3F4F7 );
        T[ Tok::Surface ]      = Color::Hex( 0xFFFFFF );
        T[ Tok::SurfaceAlt ]   = Color::Hex( 0xF0F2F5 );
        T[ Tok::Raised ]       = Color::Hex( 0xE4E7EC );
        T[ Tok::Border ]       = Color::Hex( 0xDCE0E6 );
        T[ Tok::BorderStrong ] = Color::Hex( 0xB9C0CB );
        T[ Tok::Text ]         = Color::Hex( 0x1B1F27 );
        T[ Tok::TextDim ]      = Color::Hex( 0x667085 );
        T[ Tok::TextOnAccent ] = Color::Hex( 0xFFFFFF );
        T[ Tok::Accent ]       = Color::Hex( 0x2B7FA6 );
        T[ Tok::AccentHover ]  = Color::Hex( 0x3591BC );
        T[ Tok::Success ]      = Color::Hex( 0x23935A );
        T[ Tok::Warning ]      = Color::Hex( 0xC7851B );
        T[ Tok::Danger ]       = Color::Hex( 0xD03B33 );
        return T;
    }

    Theme& GetTheme()
    {
        static Theme T = Theme::Dark();
        return T;
    }

    const char* Theme::TokenName( Tok T )
    {
        static const char* N[] = { "Background", "Surface", "SurfaceAlt", "Raised", "Border", "BorderStrong", "Text", "TextDim",
                                   "TextOnAccent", "Accent", "AccentHover", "Success", "Warning", "Danger" };
        static_assert( sizeof( N ) / sizeof( N[ 0 ] ) == size_t( Tok::Count ), "token names out of sync" );
        return N[ size_t( T ) ];
    }

    std::string Theme::ToJson() const
    {
        std::string S = "{\n";
        char B[ 64 ];
        for ( size_t i = 0; i < size_t( Tok::Count ); i++ )
        {
            Color C = Colors[ i ];
            if ( C.a == 255 ) std::snprintf( B, sizeof( B ), "  \"%s\": \"#%02X%02X%02X\",\n", TokenName( Tok( i ) ), C.r, C.g, C.b );
            else std::snprintf( B, sizeof( B ), "  \"%s\": \"#%02X%02X%02X%02X\",\n", TokenName( Tok( i ) ), C.r, C.g, C.b, C.a );
            S += B;
        }
        std::snprintf( B, sizeof( B ), "  \"Radius\": %g\n}\n", Radius );
        return S + B;
    }

    bool Theme::FromJson( const std::string& J )
    {
        // a tiny reader for flat { "Key": "#hex" | number } objects
        bool Any = false;
        for ( size_t i = 0; ( i = J.find( '"', i ) ) != std::string::npos; )
        {
            size_t E = J.find( '"', i + 1 );
            if ( E == std::string::npos ) break;
            std::string Key = J.substr( i + 1, E - i - 1 );
            size_t Colon = J.find_first_not_of( " \t\r\n", E + 1 );
            if ( Colon == std::string::npos || J[ Colon ] != ':' ) { i = E + 1; continue; }
            size_t V = J.find_first_not_of( " \t\r\n", Colon + 1 );
            if ( V == std::string::npos ) break;
            if ( J[ V ] == '"' )
            {
                size_t Ve = J.find( '"', V + 1 );
                if ( Ve == std::string::npos ) break;
                std::string Val = J.substr( V + 1, Ve - V - 1 );
                if ( !Val.empty() && Val[ 0 ] == '#' )
                {
                    unsigned long H = std::strtoul( Val.c_str() + 1, nullptr, 16 );
                    Color C = Val.size() == 9 ? Color( uint8_t( H >> 24 ), uint8_t( H >> 16 ), uint8_t( H >> 8 ), uint8_t( H ) ) : Color::Hex( uint32_t( H ) );
                    for ( size_t t = 0; t < size_t( Tok::Count ); t++ )
                        if ( Key == TokenName( Tok( t ) ) ) { Colors[ t ] = C; Any = true; }
                }
                i = Ve + 1;
            }
            else
            {
                if ( Key == "Radius" ) { Radius = float( std::atof( J.c_str() + V ) ); Any = true; }
                i = V + 1;
            }
        }
        return Any;
    }

    bool LoadThemeFile( const std::string& Path )
    {
        std::ifstream F( Path, std::ios::binary );
        if ( !F ) return false;
        std::stringstream S;
        S << F.rdbuf();
        return GetTheme().FromJson( S.str() );
    }

    bool SaveThemeFile( const std::string& Path )
    {
        std::ofstream F( Path, std::ios::binary );
        if ( !F ) return false;
        F << GetTheme().ToJson();
        return bool( F );
    }

    std::string ThemeFileStamp( const std::string& Path )
    {
        std::error_code Ec;
        auto T = std::filesystem::last_write_time( Path, Ec );
        if ( Ec ) return {};
        auto Sz = std::filesystem::file_size( Path, Ec );
        return std::to_string( T.time_since_epoch().count() ) + ":" + std::to_string( Sz );
    }
}
