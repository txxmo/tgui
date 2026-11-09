#include "tg/input.h"

namespace tg
{
    const char* KeyName( Key K )
    {
        static const char* const Names[] = {
            "NONE",
            "TAB", "ENTER", "ESC", "BACKSPACE", "DELETE", "INSERT", "SPACE",
            "LEFT", "RIGHT", "UP", "DOWN", "HOME", "END", "PGUP", "PGDN",
            "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
            "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
            "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
            "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24",
            "'", ",", "-", ".", "/", ";", "=", "[", "\\", "]", "`",
            "CAPS", "SCRLK", "NUMLK", "PRTSC", "PAUSE", "MENU",
            "NUM0", "NUM1", "NUM2", "NUM3", "NUM4", "NUM5", "NUM6", "NUM7", "NUM8", "NUM9",
            "NUM.", "NUM/", "NUM*", "NUM-", "NUM+", "NUMENTER", "NUM=",
            "LSHIFT", "LCTRL", "LALT", "LSUPER", "RSHIFT", "RCTRL", "RALT", "RSUPER",
            "MOUSE1", "MOUSE2", "MOUSE3", "MOUSE4", "MOUSE5",
            "?",
        };
        static_assert( sizeof( Names ) / sizeof( Names[ 0 ] ) == size_t( Key::Count ), "KeyName table out of sync with tg::Key" );
        size_t i = size_t( K );
        return i < size_t( Key::Count ) ? Names[ i ] : "?";
    }
}
