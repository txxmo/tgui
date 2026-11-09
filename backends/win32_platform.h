// Win32 platform glue: Windows messages -> tg::Context input, plus DPI, cursor, clipboard and an
// event-driven wait. Works for a window you own (tg::App, examples/win32_*) and for a window you
// don't (hook its WndProc, call HandleMessage first, like the old tgui Input::HandleMouse).
//
//   LRESULT CALLBACK WndProc( HWND h, UINT m, WPARAM w, LPARAM l )
//   {
//       if ( tg::win32::HandleMessage( h, m, w, l ) ) return 0;   // tg used it
//       ...
//   }
//
// No <windows.h> in this header: HWND/WPARAM/LPARAM are passed as plain integers/pointers.
#pragma once
#include <cstdint>

namespace tg
{
    class Context;

    namespace win32
    {
        // Attach Ctx to a window: clipboard, wake-up, cursor, file drops (OLE), IME. Any number of
        // windows, one Context each.
        void Install( void* Hwnd, Context& Ctx );
        void Uninstall( void* Hwnd ); // before the window or its Context goes away
        void Shutdown();              // every window

        // Feed one message. Returns true when tg consumed it (mouse over a tg widget, key while a
        // tg text field / keybind has focus...). For an overlay, skip the game's input when true.
        bool HandleMessage( void* Hwnd, uint32_t Msg, uintptr_t WParam, intptr_t LParam );

        // Keep viewport + DPI scale + cursor in sync. Call once per loop iteration before Frame().
        void Sync( void* Hwnd, Context& Ctx );

        // Sleep until there's input, a posted task, an animation frame or a timed redraw. Pumps nothing.
        void WaitForWork( Context& Ctx );

        float GetDpiScale( void* Hwnd );
        void EnableDpiAwareness(); // per-monitor v2 if available; call before creating windows
    }
}
