// tg/input.h — platform-neutral input events. Platform glue (GLFW, Win32, SDL...) translates into these.
#pragma once
#include "core.h"

namespace tg
{
    enum class Key : uint16_t
    {
        None,
        Tab, Enter, Escape, Backspace, Delete, Space,
        Left, Right, Up, Down, Home, End, PageUp, PageDown,
        A, C, V, X, Z,
        Unknown,
    };

    enum Mod : uint8_t { ModShift = 1, ModCtrl = 2, ModAlt = 4, ModSuper = 8 };

    enum class MouseButton : uint8_t { Left, Right, Middle };

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
