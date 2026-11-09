// Small helpers shared by several source files (inline: one definition, also in the single-header build).
#pragma once
#include "tg/font.h"
#include <cstdint>
#include <string>

namespace tg::internal
{
    inline constexpr float Pi = 3.14159265358979f;

    inline void AppendUtf8( std::string& S, uint32_t C )
    {
        if ( C < 0x80 ) S += char( C );
        else if ( C < 0x800 ) { S += char( 0xC0 | C >> 6 ); S += char( 0x80 | ( C & 0x3F ) ); }
        else if ( C < 0x10000 ) { S += char( 0xE0 | C >> 12 ); S += char( 0x80 | ( ( C >> 6 ) & 0x3F ) ); S += char( 0x80 | ( C & 0x3F ) ); }
        else { S += char( 0xF0 | C >> 18 ); S += char( 0x80 | ( ( C >> 12 ) & 0x3F ) ); S += char( 0x80 | ( ( C >> 6 ) & 0x3F ) ); S += char( 0x80 | ( C & 0x3F ) ); }
    }

    // caret movement in text fields: whole graphemes (emoji sequences, flags) at a time
    inline size_t PrevCp( const std::string& S, size_t i ) { return PrevGrapheme( S, i ); }
    inline size_t NextCp( const std::string& S, size_t i ) { return NextGrapheme( S, i ); }
}
