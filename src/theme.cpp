#include "tg/theme.h"

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
}
