#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <imm.h>
#include <ole2.h>
#include <shellapi.h>

#include "win32_platform.h"
#include "tg/context.h"
#include <string>
#include <unordered_map>
#include <vector>

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
        struct DropTarget;
        struct State
        {
            Context* Ctx = nullptr;
            Cursor CurCursor = Cursor::Arrow;
            wchar_t HighSurrogate = 0;
            int ButtonsDown = 0;
            DropTarget* Drop = nullptr;
            bool Ole = false;
        };
        std::unordered_map<HWND, State> States; // one per window (App::NewWindow, pop-outs)
        State* Find( HWND H )
        {
            auto It = States.find( H );
            return It == States.end() ? nullptr : &It->second;
        }

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

        void ApplyCursor( Cursor C ) { SetCursor( LoadCursorW( nullptr, CursorName( C ) ) ); }

        // Is the mouse over something tg cares about? (for overlays deciding who gets input)
        bool OverUI( Context& C ) { return C.Hovered() || C.Pressed() || C.HasPopups() || C.FilesOver(); }
        bool WantsKeys( Context& C ) { return C.Focused() || C.Captured(); }
        bool Typing( Context& C ) { return C.Focused() && C.Focused()->WantsKeyboard(); }

        std::string Narrow( const wchar_t* W, int Len = -1 )
        {
            int N = WideCharToMultiByte( CP_UTF8, 0, W, Len, nullptr, 0, nullptr, nullptr );
            std::string S( size_t( N > 0 ? N : 0 ), '\0' );
            if ( N > 0 ) WideCharToMultiByte( CP_UTF8, 0, W, Len, S.data(), N, nullptr, nullptr );
            if ( Len < 0 && !S.empty() ) S.pop_back();
            return S;
        }

        std::vector<std::string> DropPaths( HDROP D )
        {
            std::vector<std::string> Out;
            UINT N = DragQueryFileW( D, 0xFFFFFFFF, nullptr, 0 );
            for ( UINT i = 0; i < N; i++ )
            {
                UINT Len = DragQueryFileW( D, i, nullptr, 0 );
                std::wstring W( Len, L'\0' );
                DragQueryFileW( D, i, W.data(), Len + 1 );
                Out.push_back( Narrow( W.c_str() ) );
            }
            return Out;
        }

        // OLE drop target: files dragged over the window highlight the widget that would take them.
        struct DropTarget : IDropTarget
        {
            LONG Refs = 1;
            HWND Wnd;
            std::vector<std::string> Paths;
            explicit DropTarget( HWND H ) : Wnd( H ) {}

            HRESULT STDMETHODCALLTYPE QueryInterface( REFIID Id, void** Out ) override
            {
                if ( Id == IID_IUnknown || Id == IID_IDropTarget ) { *Out = static_cast<IDropTarget*>( this ); AddRef(); return S_OK; }
                *Out = nullptr;
                return E_NOINTERFACE;
            }
            ULONG STDMETHODCALLTYPE AddRef() override { return ULONG( InterlockedIncrement( &Refs ) ); }
            ULONG STDMETHODCALLTYPE Release() override
            {
                LONG R = InterlockedDecrement( &Refs );
                if ( !R ) delete this;
                return ULONG( R );
            }

            Vec2 At( POINTL P )
            {
                POINT Q = { P.x, P.y };
                ScreenToClient( Wnd, &Q );
                float S = ScaleOf( Wnd );
                return { Q.x / S, Q.y / S };
            }
            static std::vector<std::string> Read( IDataObject* D )
            {
                std::vector<std::string> Out;
                FORMATETC Fe = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
                STGMEDIUM Sm = {};
                if ( D && SUCCEEDED( D->GetData( &Fe, &Sm ) ) )
                {
                    if ( HDROP H = (HDROP)GlobalLock( Sm.hGlobal ) )
                    {
                        Out = DropPaths( H );
                        GlobalUnlock( Sm.hGlobal );
                    }
                    ReleaseStgMedium( &Sm );
                }
                return Out;
            }
            Context* Ctx()
            {
                State* St = Find( Wnd );
                return St ? St->Ctx : nullptr;
            }
            HRESULT STDMETHODCALLTYPE DragEnter( IDataObject* D, DWORD, POINTL P, DWORD* Eff ) override
            {
                Paths = Read( D );
                Context* C = Ctx();
                *Eff = C && !Paths.empty() && C->OnFilesOver( At( P ), Paths ) ? DROPEFFECT_COPY : DROPEFFECT_NONE;
                return S_OK;
            }
            HRESULT STDMETHODCALLTYPE DragOver( DWORD, POINTL P, DWORD* Eff ) override
            {
                Context* C = Ctx();
                *Eff = C && !Paths.empty() && C->OnFilesOver( At( P ), Paths ) ? DROPEFFECT_COPY : DROPEFFECT_NONE;
                return S_OK;
            }
            HRESULT STDMETHODCALLTYPE DragLeave() override
            {
                if ( Context* C = Ctx() ) C->OnFilesLeave();
                Paths.clear();
                return S_OK;
            }
            HRESULT STDMETHODCALLTYPE Drop( IDataObject* D, DWORD, POINTL P, DWORD* Eff ) override
            {
                auto Ps = Read( D );
                if ( Ps.empty() ) Ps = Paths;
                Context* C = Ctx();
                *Eff = C && !Ps.empty() && C->OnFilesDrop( At( P ), Ps ) ? DROPEFFECT_COPY : DROPEFFECT_NONE;
                Paths.clear();
                return S_OK;
            }
        };

        // Put the IME's composition / candidate windows at the caret of the focused text widget.
        void PlaceIme( HWND H, Context& C )
        {
            Rect R = C.CaretRect();
            if ( R.h <= 0 ) return;
            HIMC Imc = ImmGetContext( H );
            if ( !Imc ) return;
            float S = ScaleOf( H );
            COMPOSITIONFORM Cf = {};
            Cf.dwStyle = CFS_POINT;
            Cf.ptCurrentPos = { LONG( R.x * S ), LONG( R.y * S ) };
            ImmSetCompositionWindow( Imc, &Cf );
            CANDIDATEFORM Cand = {};
            Cand.dwIndex = 0;
            Cand.dwStyle = CFS_EXCLUDE;
            Cand.ptCurrentPos = { LONG( R.x * S ), LONG( R.Bottom() * S ) };
            Cand.rcArea = { LONG( R.x * S ), LONG( R.y * S ), LONG( ( R.x + 1 ) * S ), LONG( R.Bottom() * S ) };
            ImmSetCandidateWindow( Imc, &Cand );
            ImmReleaseContext( H, Imc );
        }
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

    void Install( void* Hv, Context& C )
    {
        HWND Wnd = (HWND)Hv;
        State& St = States[ Wnd ];
        St = State{};
        St.Ctx = &C;
        C.NativeWindow = Hv;
        C.WakeUp = [ Wnd ] { PostMessageW( Wnd, WM_NULL, 0, 0 ); };
        // files from Explorer: OLE drop target (hover feedback), or plain WM_DROPFILES if OLE isn't available
        HRESULT Ole = OleInitialize( nullptr );
        St.Ole = SUCCEEDED( Ole );
        if ( St.Ole )
        {
            St.Drop = new DropTarget( Wnd );
            if ( FAILED( RegisterDragDrop( Wnd, St.Drop ) ) ) { St.Drop->Release(); St.Drop = nullptr; }
        }
        if ( !St.Drop ) DragAcceptFiles( Wnd, TRUE );
        C.SetClipboard = [ Wnd ]( const std::string& S ) {
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
        C.GetClipboard = [ Wnd ]() -> std::string {
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
        Sync( Hv, C );
    }

    void Uninstall( void* Hv )
    {
        HWND H = (HWND)Hv;
        State* St = Find( H );
        if ( !St ) return;
        if ( St->Ctx ) { St->Ctx->WakeUp = nullptr; St->Ctx->SetClipboard = nullptr; St->Ctx->GetClipboard = nullptr; }
        if ( St->Drop )
        {
            RevokeDragDrop( H );
            St->Drop->Release();
        }
        if ( St->Ole ) OleUninitialize();
        States.erase( H );
    }

    void Shutdown()
    {
        std::vector<HWND> All;
        for ( auto& [ H, St ] : States ) All.push_back( H );
        for ( HWND H : All ) Uninstall( H );
    }

    bool HandleMessage( void* Hv, uint32_t Msg, uintptr_t W, intptr_t L )
    {
        HWND H = (HWND)Hv;
        State* St = Find( H );
        if ( !St || !St->Ctx ) return false;
        Context& C = *St->Ctx;
        int& ButtonsDown = St->ButtonsDown;
        wchar_t& HighSurrogate = St->HighSurrogate;

        auto Button = [ & ]( MouseButton B, bool Down ) {
            if ( Down ) { if ( !ButtonsDown++ ) SetCapture( H ); }
            else if ( ButtonsDown > 0 && !--ButtonsDown ) ReleaseCapture();
            bool Was = OverUI( C );
            C.OnMouseMove( ToUI( H, L ) );
            C.OnMouseButton( B, Down );
            return Was || OverUI( C );
        };

        switch ( Msg )
        {
        case WM_MOUSEMOVE:
        {
            TRACKMOUSEEVENT T = { sizeof( T ), TME_LEAVE, H, 0 };
            TrackMouseEvent( &T );
            C.OnMouseMove( ToUI( H, L ) );
            return OverUI( C );
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
            bool Over = OverUI( C );
            C.OnMouseWheel( Msg == WM_MOUSEWHEEL ? Vec2{ 0, D } : Vec2{ -D, 0 } );
            return Over;
        }
        case WM_KEYDOWN: case WM_SYSKEYDOWN:
        case WM_KEYUP: case WM_SYSKEYUP:
        {
            bool Down = Msg == WM_KEYDOWN || Msg == WM_SYSKEYDOWN;
            bool Repeat = Down && ( L & ( 1 << 30 ) );
            bool Wanted = WantsKeys( C );
            C.OnKey( MapVK( W, L ), Down, Mods(), Repeat );
            return Wanted;
        }
        case WM_CHAR:
        {
            wchar_t Ch = wchar_t( W );
            if ( Ch >= 0xD800 && Ch <= 0xDBFF ) { HighSurrogate = Ch; return WantsKeys( C ); }
            uint32_t Cp = Ch;
            if ( Ch >= 0xDC00 && Ch <= 0xDFFF && HighSurrogate ) Cp = 0x10000 + ( ( HighSurrogate - 0xD800 ) << 10 ) + ( Ch - 0xDC00 );
            HighSurrogate = 0;
            bool Wanted = WantsKeys( C );
            if ( Cp >= 32 ) C.OnText( Cp );
            return Wanted;
        }
        case WM_SETCURSOR:
            if ( LOWORD( L ) == HTCLIENT ) { ApplyCursor( St->CurCursor ); return true; }
            return false;
        case WM_KILLFOCUS:
            ButtonsDown = 0;
            C.OnComposition( {} );
            return false;
        case WM_PAINT:
            C.RequestFrame();
            return false;
        case WM_DROPFILES: // (only when OLE drag & drop couldn't be registered)
        {
            HDROP D = (HDROP)W;
            POINT P = {};
            DragQueryPoint( D, &P );
            float S = ScaleOf( H );
            C.OnFilesDrop( { P.x / S, P.y / S }, DropPaths( D ) );
            DragFinish( D );
            return true;
        }
        // ---- IME: tg draws the text being composed at the caret; the candidate list follows the caret
        case WM_IME_STARTCOMPOSITION:
            if ( !Typing( C ) ) return false;
            PlaceIme( H, C );
            return true; // (no system composition window)
        case WM_IME_COMPOSITION:
        {
            if ( !Typing( C ) ) return false;
            HIMC Imc = ImmGetContext( H );
            if ( !Imc ) return false;
            if ( L & GCS_RESULTSTR ) // committed: type it
            {
                LONG Bytes = ImmGetCompositionStringW( Imc, GCS_RESULTSTR, nullptr, 0 );
                std::wstring R( size_t( Bytes > 0 ? Bytes : 0 ) / sizeof( wchar_t ), L'\0' );
                if ( Bytes > 0 ) ImmGetCompositionStringW( Imc, GCS_RESULTSTR, R.data(), DWORD( Bytes ) );
                C.OnComposition( {} );
                for ( size_t i = 0; i < R.size(); i++ )
                {
                    uint32_t Cp = R[ i ];
                    if ( Cp >= 0xD800 && Cp <= 0xDBFF && i + 1 < R.size() ) Cp = 0x10000 + ( ( Cp - 0xD800 ) << 10 ) + ( R[ ++i ] - 0xDC00 );
                    if ( Cp >= 32 ) C.OnText( Cp );
                }
            }
            if ( L & GCS_COMPSTR )
            {
                LONG Bytes = ImmGetCompositionStringW( Imc, GCS_COMPSTR, nullptr, 0 );
                std::wstring Cs( size_t( Bytes > 0 ? Bytes : 0 ) / sizeof( wchar_t ), L'\0' );
                if ( Bytes > 0 ) ImmGetCompositionStringW( Imc, GCS_COMPSTR, Cs.data(), DWORD( Bytes ) );
                int Cur = -1;
                if ( L & GCS_CURSORPOS )
                {
                    LONG Wc = ImmGetCompositionStringW( Imc, GCS_CURSORPOS, nullptr, 0 );
                    if ( Wc >= 0 && size_t( Wc ) <= Cs.size() ) Cur = int( Narrow( Cs.c_str(), int( Wc ) ).size() );
                }
                C.OnComposition( Narrow( Cs.c_str(), int( Cs.size() ) ), Cur );
            }
            ImmReleaseContext( H, Imc );
            PlaceIme( H, C );
            return true;
        }
        case WM_IME_ENDCOMPOSITION:
            C.OnComposition( {} );
            return Typing( C );
        case WM_IME_SETCONTEXT:
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

        State* St = Find( H );
        Cursor Want = C.GetCursor();
        if ( St && Want != St->CurCursor )
        {
            St->CurCursor = Want;
            POINT P;
            GetCursorPos( &P );
            if ( WindowFromPoint( P ) == H ) ApplyCursor( Want );
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
