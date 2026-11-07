#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>

#include "win32_platform.h"
#include "tg/context.h"
#include <string>

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif

namespace tg::win32
{
    namespace
    {
        HWND Wnd = nullptr;
        Context* Ctx = nullptr;
        Cursor CurCursor = Cursor::Arrow;
        wchar_t HighSurrogate = 0;
        int ButtonsDown = 0;

        float ScaleOf( HWND H )
        {
            using Fn = UINT( WINAPI* )( HWND );
            static Fn GetDpiForWindowFn = (Fn)(void*)GetProcAddress( GetModuleHandleW( L"user32.dll" ), "GetDpiForWindow" );
            UINT Dpi = GetDpiForWindowFn && H ? GetDpiForWindowFn( H ) : 96;
            return Dpi ? Dpi / 96.f : 1.f;
        }

        Vec2 ToUI( HWND H, LPARAM L )
        {
            float S = ScaleOf( H );
            return { GET_X_LPARAM( L ) / S, GET_Y_LPARAM( L ) / S };
        }

        uint8_t Mods()
        {
            uint8_t M = 0;
            if ( GetKeyState( VK_SHIFT ) & 0x8000 ) M |= ModShift;
            if ( GetKeyState( VK_CONTROL ) & 0x8000 ) M |= ModCtrl;
            if ( GetKeyState( VK_MENU ) & 0x8000 ) M |= ModAlt;
            if ( ( GetKeyState( VK_LWIN ) | GetKeyState( VK_RWIN ) ) & 0x8000 ) M |= ModSuper;
            return M;
        }

        Key MapVK( WPARAM Vk, LPARAM L )
        {
            const bool Ext = ( L >> 24 ) & 1;
            const UINT Scan = ( L >> 16 ) & 0xFF;
            if ( Vk >= 'A' && Vk <= 'Z' ) return Key( uint16_t( Key::A ) + ( Vk - 'A' ) );
            if ( Vk >= '0' && Vk <= '9' ) return Key( uint16_t( Key::Num0 ) + ( Vk - '0' ) );
            if ( Vk >= VK_F1 && Vk <= VK_F24 ) return Key( uint16_t( Key::F1 ) + ( Vk - VK_F1 ) );
            if ( Vk >= VK_NUMPAD0 && Vk <= VK_NUMPAD9 ) return Key( uint16_t( Key::Kp0 ) + ( Vk - VK_NUMPAD0 ) );
            switch ( Vk )
            {
            case VK_TAB: return Key::Tab;
            case VK_RETURN: return Ext ? Key::KpEnter : Key::Enter;
            case VK_ESCAPE: return Key::Escape;
            case VK_BACK: return Key::Backspace;
            case VK_DELETE: return Key::Delete;
            case VK_INSERT: return Key::Insert;
            case VK_SPACE: return Key::Space;
            case VK_LEFT: return Key::Left;
            case VK_RIGHT: return Key::Right;
            case VK_UP: return Key::Up;
            case VK_DOWN: return Key::Down;
            case VK_HOME: return Key::Home;
            case VK_END: return Key::End;
            case VK_PRIOR: return Key::PageUp;
            case VK_NEXT: return Key::PageDown;
            case VK_OEM_7: return Key::Apostrophe;
            case VK_OEM_COMMA: return Key::Comma;
            case VK_OEM_MINUS: return Key::Minus;
            case VK_OEM_PERIOD: return Key::Period;
            case VK_OEM_2: return Key::Slash;
            case VK_OEM_1: return Key::Semicolon;
            case VK_OEM_PLUS: return Key::Equal;
            case VK_OEM_4: return Key::LeftBracket;
            case VK_OEM_5: return Key::Backslash;
            case VK_OEM_6: return Key::RightBracket;
            case VK_OEM_3: return Key::GraveAccent;
            case VK_CAPITAL: return Key::CapsLock;
            case VK_SCROLL: return Key::ScrollLock;
            case VK_NUMLOCK: return Key::NumLock;
            case VK_SNAPSHOT: return Key::PrintScreen;
            case VK_PAUSE: return Key::Pause;
            case VK_APPS: return Key::Menu;
            case VK_DECIMAL: return Key::KpDecimal;
            case VK_DIVIDE: return Key::KpDivide;
            case VK_MULTIPLY: return Key::KpMultiply;
            case VK_SUBTRACT: return Key::KpSubtract;
            case VK_ADD: return Key::KpAdd;
            case VK_SHIFT: return MapVirtualKeyW( Scan, MAPVK_VSC_TO_VK_EX ) == VK_RSHIFT ? Key::RightShift : Key::LeftShift;
            case VK_CONTROL: return Ext ? Key::RightCtrl : Key::LeftCtrl;
            case VK_MENU: return Ext ? Key::RightAlt : Key::LeftAlt;
            case VK_LWIN: return Key::LeftSuper;
            case VK_RWIN: return Key::RightSuper;
            default: return Key::Unknown;
            }
        }

        LPCWSTR CursorName( Cursor C )
        {
            switch ( C )
            {
            case Cursor::Hand: return MAKEINTRESOURCEW( 32649 ); // IDC_HAND (spelled out: IDC_* are narrow strings unless UNICODE is defined)
            case Cursor::IBeam: return MAKEINTRESOURCEW( 32513 ); // IDC_IBEAM
            case Cursor::ResizeH: return MAKEINTRESOURCEW( 32644 ); // IDC_SIZEWE
            case Cursor::ResizeV: return MAKEINTRESOURCEW( 32645 ); // IDC_SIZENS
            default: return MAKEINTRESOURCEW( 32512 ); // IDC_ARROW
            }
        }

        void ApplyCursor() { SetCursor( LoadCursorW( nullptr, CursorName( CurCursor ) ) ); }

        // Is the mouse over something tg cares about? (for overlays deciding who gets input)
        bool OverUI() { return Ctx && ( Ctx->Hovered() || Ctx->Pressed() || Ctx->HasPopups() ); }
        bool WantsKeys() { return Ctx && ( Ctx->Focused() || Ctx->Captured() ); }
    }

    void EnableDpiAwareness()
    {
        HMODULE U = GetModuleHandleW( L"user32.dll" );
        using CtxFn = BOOL( WINAPI* )( HANDLE );
        if ( auto F = (CtxFn)(void*)GetProcAddress( U, "SetProcessDpiAwarenessContext" ) )
            if ( F( (HANDLE)-4 /* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 */ ) ) return;
        using OldFn = BOOL( WINAPI* )();
        if ( auto F = (OldFn)(void*)GetProcAddress( U, "SetProcessDPIAware" ) ) F();
    }

    float GetDpiScale( void* H ) { return ScaleOf( (HWND)H ); }

    void Install( void* H, Context& C )
    {
        Wnd = (HWND)H;
        Ctx = &C;
        C.NativeWindow = H;
        C.WakeUp = [] { if ( Wnd ) PostMessageW( Wnd, WM_NULL, 0, 0 ); };
        C.SetClipboard = []( const std::string& S ) {
            if ( !OpenClipboard( Wnd ) ) return;
            EmptyClipboard();
            int N = MultiByteToWideChar( CP_UTF8, 0, S.c_str(), -1, nullptr, 0 );
            if ( HGLOBAL G = GlobalAlloc( GMEM_MOVEABLE, size_t( N ) * sizeof( wchar_t ) ) )
            {
                MultiByteToWideChar( CP_UTF8, 0, S.c_str(), -1, (wchar_t*)GlobalLock( G ), N );
                GlobalUnlock( G );
                if ( !SetClipboardData( CF_UNICODETEXT, G ) ) GlobalFree( G );
            }
            CloseClipboard();
        };
        C.GetClipboard = []() -> std::string {
            std::string Out;
            if ( !OpenClipboard( Wnd ) ) return Out;
            if ( HANDLE D = GetClipboardData( CF_UNICODETEXT ) )
                if ( const wchar_t* W = (const wchar_t*)GlobalLock( D ) )
                {
                    int N = WideCharToMultiByte( CP_UTF8, 0, W, -1, nullptr, 0, nullptr, nullptr );
                    if ( N > 1 ) { Out.resize( size_t( N ) ); WideCharToMultiByte( CP_UTF8, 0, W, -1, Out.data(), N, nullptr, nullptr ); Out.pop_back(); }
                    GlobalUnlock( D );
                }
            CloseClipboard();
            return Out;
        };
        Sync( H, C );
    }

    void Shutdown()
    {
        if ( Ctx ) { Ctx->WakeUp = nullptr; Ctx->SetClipboard = nullptr; Ctx->GetClipboard = nullptr; }
        Ctx = nullptr;
        Wnd = nullptr;
    }

    bool HandleMessage( void* Hv, uint32_t Msg, uintptr_t W, intptr_t L )
    {
        HWND H = (HWND)Hv;
        if ( !Ctx || H != Wnd ) return false;
        Context& C = *Ctx;

        auto Button = [ & ]( MouseButton B, bool Down ) {
            if ( Down ) { if ( !ButtonsDown++ ) SetCapture( H ); }
            else if ( ButtonsDown > 0 && !--ButtonsDown ) ReleaseCapture();
            bool Was = OverUI();
            C.OnMouseMove( ToUI( H, L ) );
            C.OnMouseButton( B, Down );
            return Was || OverUI();
        };

        switch ( Msg )
        {
        case WM_MOUSEMOVE:
        {
            TRACKMOUSEEVENT T = { sizeof( T ), TME_LEAVE, H, 0 };
            TrackMouseEvent( &T );
            C.OnMouseMove( ToUI( H, L ) );
            return OverUI();
        }
        case WM_MOUSELEAVE:
            C.OnMouseMove( { -100000, -100000 } );
            return false;
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: return Button( MouseButton::Left, true );
        case WM_LBUTTONUP: return Button( MouseButton::Left, false );
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: return Button( MouseButton::Right, true );
        case WM_RBUTTONUP: return Button( MouseButton::Right, false );
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: return Button( MouseButton::Middle, true );
        case WM_MBUTTONUP: return Button( MouseButton::Middle, false );
        case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
        case WM_XBUTTONUP:
        {
            MouseButton B = HIWORD( W ) == XBUTTON1 ? MouseButton::Back : MouseButton::Forward;
            Button( B, Msg != WM_XBUTTONUP );
            return true; // XBUTTON messages must return TRUE
        }
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        {
            float D = float( GET_WHEEL_DELTA_WPARAM( W ) ) / WHEEL_DELTA;
            bool Over = OverUI();
            C.OnMouseWheel( Msg == WM_MOUSEWHEEL ? Vec2{ 0, D } : Vec2{ -D, 0 } );
            return Over;
        }
        case WM_KEYDOWN: case WM_SYSKEYDOWN:
        case WM_KEYUP: case WM_SYSKEYUP:
        {
            bool Down = Msg == WM_KEYDOWN || Msg == WM_SYSKEYDOWN;
            bool Repeat = Down && ( L & ( 1 << 30 ) );
            bool Wanted = WantsKeys();
            C.OnKey( MapVK( W, L ), Down, Mods(), Repeat );
            return Wanted;
        }
        case WM_CHAR:
        {
            wchar_t Ch = wchar_t( W );
            if ( Ch >= 0xD800 && Ch <= 0xDBFF ) { HighSurrogate = Ch; return WantsKeys(); }
            uint32_t Cp = Ch;
            if ( Ch >= 0xDC00 && Ch <= 0xDFFF && HighSurrogate ) Cp = 0x10000 + ( ( HighSurrogate - 0xD800 ) << 10 ) + ( Ch - 0xDC00 );
            HighSurrogate = 0;
            bool Wanted = WantsKeys();
            if ( Cp >= 32 ) C.OnText( Cp );
            return Wanted;
        }
        case WM_SETCURSOR:
            if ( LOWORD( L ) == HTCLIENT ) { ApplyCursor(); return true; }
            return false;
        case WM_KILLFOCUS:
            ButtonsDown = 0;
            return false;
        case WM_PAINT:
            C.RequestFrame();
            return false;
        default:
            return false;
        }
    }

    void Sync( void* Hv, Context& C )
    {
        HWND H = (HWND)Hv;
        RECT R;
        GetClientRect( H, &R );
        float S = ScaleOf( H );
        C.SetViewport( { ( R.right - R.left ) / S, ( R.bottom - R.top ) / S }, S );

        Cursor Want = C.GetCursor();
        if ( Want != CurCursor )
        {
            CurCursor = Want;
            POINT P;
            GetCursorPos( &P );
            if ( WindowFromPoint( P ) == H ) ApplyCursor();
        }
    }

    void WaitForWork( Context& C )
    {
        double T = C.SecondsUntilNextFrame();
        if ( T > 0 )
        {
            DWORD Ms = T > 0.5 ? 500 : DWORD( T * 1000 );
            MsgWaitForMultipleObjectsEx( 0, nullptr, Ms, QS_ALLINPUT, MWMO_INPUTAVAILABLE );
        }
        C.RunPumps();
    }
}
