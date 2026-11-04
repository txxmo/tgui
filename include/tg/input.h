// tg/input.h — platform-neutral input events. Platform glue (GLFW, Win32, SDL...) translates into these.
#pragma once
#include "core.h"

namespace tg
{
    // Every key a platform layer can report, plus mouse buttons (so a Keybind can be "Mouse 4").
    enum class Key : uint16_t
    {
        None,
        // editing / navigation
        Tab, Enter, Escape, Backspace, Delete, Insert, Space,
        Left, Right, Up, Down, Home, End, PageUp, PageDown,
        // letters and digits
        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
        // function keys
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
        F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24,
        // punctuation
        Apostrophe, Comma, Minus, Period, Slash, Semicolon, Equal, LeftBracket, Backslash, RightBracket, GraveAccent,
        // locks / system
        CapsLock, ScrollLock, NumLock, PrintScreen, Pause, Menu,
        // keypad
        Kp0, Kp1, Kp2, Kp3, Kp4, Kp5, Kp6, Kp7, Kp8, Kp9,
        KpDecimal, KpDivide, KpMultiply, KpSubtract, KpAdd, KpEnter, KpEqual,
        // modifiers
        LeftShift, LeftCtrl, LeftAlt, LeftSuper, RightShift, RightCtrl, RightAlt, RightSuper,
        // mouse buttons (left, right, middle, back, forward)
        Mouse1, Mouse2, Mouse3, Mouse4, Mouse5,
        Unknown,
        Count
    };

    // Short display name: "A", "F5", "LSHIFT", "MOUSE4", "SPACE"... ("NONE" for Key::None).
    const char* KeyName( Key K );

    enum Mod : uint8_t { ModShift = 1, ModCtrl = 2, ModAlt = 4, ModSuper = 8 };

    enum class MouseButton : uint8_t { Left, Right, Middle, Back, Forward };
    inline Key MouseKey( MouseButton B ) { return Key( uint16_t( Key::Mouse1 ) + uint16_t( B ) ); }

    enum class EventType : uint8_t
    {
        MouseMove,  // Pos
        MouseDown,  // Pos, Button
        MouseUp,    // Pos, Button
        MouseWheel, // Pos, Wheel (lines, +y = scroll up)
        KeyDown,    // Key, Mods, Repeat
        KeyUp,      // Key, Mods
        Text,       // Codepoint (already layout/IME translated)
        MouseEnter, // synthesized by Context
        MouseLeave,
        FocusIn,
        FocusOut,
    };

    struct Event
    {
        EventType Type = EventType::MouseMove;
        Vec2 Pos;
        Vec2 Wheel;
        MouseButton Button = MouseButton::Left;
        Key KeyCode = Key::None;
        uint8_t Mods = 0;
        bool Repeat = false;
        uint32_t Codepoint = 0;
    };

    enum class Cursor : uint8_t { Arrow, Hand, IBeam, ResizeH, ResizeV };
}
