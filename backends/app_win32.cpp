// tg::App on Win32 + Direct3D 11 — the native Windows backend (no GLFW needed).
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include "tg/app.h"
#include "app_common.h"
#include "d3d11_renderer.h"
#include "win32_platform.h"
#include <cstdio>
#include <string>
#include <vector>

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

namespace tg
{
    static std::wstring Widen( const std::string& S )
    {
        int N = MultiByteToWideChar( CP_UTF8, 0, S.c_str(), -1, nullptr, 0 );
        std::wstring W( size_t( N > 0 ? N - 1 : 0 ), L'\0' );
        if ( N > 1 ) MultiByteToWideChar( CP_UTF8, 0, S.c_str(), -1, W.data(), N );
        return W;
    }

    // One OS window: HWND + swapchain + its own Context. The main window and every NewWindow / PopOut.
    struct Win32Window final : OsWindow
    {
        App::Impl* Owner = nullptr;
        bool IsMain = false, Closing = false;
        HWND Wnd = nullptr;
        IDXGISwapChain* Swap = nullptr;
        ID3D11RenderTargetView* Rtv = nullptr;
        std::unique_ptr<Context> Ctx;
        Container* MainArea = nullptr;
        Widget* WindowLayer = nullptr;
        detail::PoppedOut Popped;

        Container& Main() override { return *MainArea; }
        Context& GetContext() override { return *Ctx; }
        void SetTitle( const std::string& T ) override { if ( Wnd ) SetWindowTextW( Wnd, Widen( T ).c_str() ); }
        void Close() override { Closing = true; }
        bool IsOpen() const override { return !Closing && Wnd; }

        void CreateRtv( ID3D11Device* Dev )
        {
            ID3D11Texture2D* Back = nullptr;
            if ( Swap && SUCCEEDED( Swap->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (void**)&Back ) ) )
            {
                Dev->CreateRenderTargetView( Back, nullptr, &Rtv );
                Back->Release();
            }
        }
        void Release()
        {
            if ( Rtv ) { Rtv->Release(); Rtv = nullptr; }
            if ( Swap ) { Swap->Release(); Swap = nullptr; }
        }
    };

    struct App::Impl
    {
        AppConfig Cfg;
        ID3D11Device* Dev = nullptr;
        ID3D11DeviceContext* Dc = nullptr;
        D3D11Renderer Dx;
        Win32Window MainWin;
        std::vector<std::unique_ptr<Win32Window>> Extra;
        int Cascade = 0;
        bool Good = false, Quit = false;
        App* Self = nullptr;

        void Resize( Win32Window& W, UINT Wd, UINT Ht )
        {
            if ( !W.Swap || Wd == 0 || Ht == 0 ) return;
            if ( W.Rtv ) { W.Rtv->Release(); W.Rtv = nullptr; }
            W.Swap->ResizeBuffers( 0, Wd, Ht, DXGI_FORMAT_UNKNOWN, 0 );
            W.CreateRtv( Dev );
            if ( W.Ctx ) W.Ctx->RequestFrame();
        }

        void DrawFrame( Win32Window& W )
        {
            if ( !W.Ctx || !W.Rtv ) return;
            win32::Sync( W.Wnd, *W.Ctx );
            if ( W.IsMain && Self->OnUpdate ) Self->OnUpdate();
            W.Ctx->Frame();
            RECT R;
            GetClientRect( W.Wnd, &R );
            Color Bg = GetTheme()[ Tok::Background ];
            const float Clear[ 4 ] = { Bg.r / 255.f, Bg.g / 255.f, Bg.b / 255.f, 1.f };
            Dc->OMSetRenderTargets( 1, &W.Rtv, nullptr );
            Dc->ClearRenderTargetView( W.Rtv, Clear );
            W.Ctx->Render( { float( R.right - R.left ), float( R.bottom - R.top ) } );
            W.Swap->Present( W.IsMain && Cfg.VSync ? 1 : 0, 0 ); // one vblank wait per loop, not one per window
        }

        bool MakeSwapChain( Win32Window& W )
        {
            IDXGIDevice* Dxgi = nullptr;
            IDXGIAdapter* Ad = nullptr;
            IDXGIFactory* Fac = nullptr;
            bool Ok = false;
            if ( SUCCEEDED( Dev->QueryInterface( __uuidof( IDXGIDevice ), (void**)&Dxgi ) ) && SUCCEEDED( Dxgi->GetAdapter( &Ad ) ) &&
                 SUCCEEDED( Ad->GetParent( __uuidof( IDXGIFactory ), (void**)&Fac ) ) )
            {
                DXGI_SWAP_CHAIN_DESC Sd = {};
                Sd.BufferCount = 2;
                Sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                Sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                Sd.OutputWindow = W.Wnd;
                Sd.SampleDesc.Count = 1;
                Sd.Windowed = TRUE;
                Sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
                Ok = SUCCEEDED( Fac->CreateSwapChain( Dev, &Sd, &W.Swap ) );
            }
            if ( Fac ) Fac->Release();
            if ( Ad ) Ad->Release();
            if ( Dxgi ) Dxgi->Release();
            if ( Ok ) W.CreateRtv( Dev );
            return Ok;
        }

        void Destroy( Win32Window& W )
        {
            if ( W.Wnd ) SetWindowLongPtrW( W.Wnd, GWLP_USERDATA, 0 );
            if ( W.Wnd ) win32::Uninstall( W.Wnd );
            W.Ctx.reset();
            W.Release();
            if ( W.Wnd && IsWindow( W.Wnd ) ) DestroyWindow( W.Wnd );
            W.Wnd = nullptr;
        }

        void Reap()
        {
            for ( size_t i = 0; i < Extra.size(); )
            {
                Win32Window& X = *Extra[ i ];
                if ( !X.Closing ) { i++; continue; }
                if ( X.OnClose ) X.OnClose();
                detail::PutBack( X.Popped, *X.MainArea );
                MainWin.Ctx->RequestFrame();
                Destroy( X );
                Extra.erase( Extra.begin() + long( i ) );
            }
        }

        static LRESULT CALLBACK WndProc( HWND H, UINT M, WPARAM W, LPARAM L )
        {
            auto* Wn = (Win32Window*)GetWindowLongPtrW( H, GWLP_USERDATA );
            // We own this window: tg sees every message, Windows still gets them (Alt+F4 etc.).
            bool Used = Wn && Wn->Ctx && win32::HandleMessage( H, M, W, L );
            if ( Used && M == WM_SETCURSOR ) return TRUE;
            if ( Used && ( M == WM_IME_STARTCOMPOSITION || M == WM_IME_COMPOSITION || M == WM_IME_ENDCOMPOSITION ) ) return 0; // tg draws it
            switch ( M )
            {
            case WM_SIZE:
                if ( Wn && W != SIZE_MINIMIZED )
                {
                    Wn->Owner->Resize( *Wn, LOWORD( L ), HIWORD( L ) );
                    Wn->Owner->DrawFrame( *Wn ); // keep painting during the modal resize loop
                }
                return 0;
            case WM_DPICHANGED:
            {
                const RECT* R = (const RECT*)L;
                SetWindowPos( H, nullptr, R->left, R->top, R->right - R->left, R->bottom - R->top, SWP_NOZORDER | SWP_NOACTIVATE );
                // the next Sync() sees the new scale; Context re-rasterizes the fonts for it
                return 0;
            }
            case WM_PAINT:
            {
                PAINTSTRUCT Ps;
                BeginPaint( H, &Ps );
                EndPaint( H, &Ps );
                if ( Wn ) Wn->Owner->DrawFrame( *Wn );
                return 0;
            }
            case WM_ERASEBKGND:
                return 1;
            case WM_SHOWWINDOW:
            case WM_ACTIVATE:
            case WM_WINDOWPOSCHANGED:
                if ( Wn && Wn->Ctx ) Wn->Ctx->RequestFrame();
                break;
            case WM_CLOSE:
                if ( Wn && !Wn->IsMain ) // an extra window: the loop tears it down
                {
                    Wn->Closing = true;
                    ShowWindow( H, SW_HIDE );
                    return 0;
                }
                if ( Wn ) Wn->Owner->Quit = true;
                DestroyWindow( H );
                return 0;
            case WM_DESTROY:
                if ( Wn && Wn->IsMain ) PostQuitMessage( 0 );
                return 0;
            }
            return DefWindowProcW( H, M, W, L );
        }

        HWND MakeWindow( const std::string& Title, int Wd, int Ht )
        {
            // Size the client area in UI units at the primary monitor's DPI.
            HDC Screen = GetDC( nullptr );
            float Scale = GetDeviceCaps( Screen, LOGPIXELSX ) / 96.f;
            ReleaseDC( nullptr, Screen );
            RECT R = { 0, 0, LONG( Wd * Scale ), LONG( Ht * Scale ) };
            AdjustWindowRectEx( &R, WS_OVERLAPPEDWINDOW, FALSE, 0 );
            return CreateWindowExW( 0, L"tg_app", Widen( Title ).c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, R.right - R.left,
                                    R.bottom - R.top, nullptr, nullptr, GetModuleHandleW( nullptr ), nullptr );
        }
    };

    App::App( const std::string& Title, int W, int H ) : App( AppConfig{ Title, W, H } ) {}

    App::App( const AppConfig& Cfg ) : P( std::make_unique<Impl>() )
    {
        P->Cfg = Cfg;
        P->Self = this;
        P->MainWin.Owner = P.get();
        P->MainWin.IsMain = true;
        win32::EnableDpiAwareness();

        WNDCLASSEXW Wc = { sizeof( Wc ) };
        Wc.style = CS_HREDRAW | CS_VREDRAW;
        Wc.lpfnWndProc = Impl::WndProc;
        Wc.hInstance = GetModuleHandleW( nullptr );
        Wc.hCursor = LoadCursorW( nullptr, MAKEINTRESOURCEW( 32512 ) ) /* IDC_ARROW */;
        Wc.lpszClassName = L"tg_app";
        RegisterClassExW( &Wc );

        Win32Window& Mw = P->MainWin;
        Mw.Wnd = P->MakeWindow( Cfg.Title, Cfg.Width, Cfg.Height );
        if ( !Mw.Wnd ) { std::fprintf( stderr, "tg: CreateWindow failed\n" ); return; }

        DXGI_SWAP_CHAIN_DESC Sd = {};
        Sd.BufferCount = 2;
        Sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        Sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        Sd.OutputWindow = Mw.Wnd;
        Sd.SampleDesc.Count = 1;
        Sd.Windowed = TRUE;
        Sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        const D3D_FEATURE_LEVEL Levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        HRESULT Hr = D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, Levels, 2, D3D11_SDK_VERSION, &Sd, &Mw.Swap, &P->Dev, nullptr, &P->Dc );
        if ( FAILED( Hr ) ) // no GPU driver (VMs, CI): fall back to the WARP software rasterizer
            Hr = D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, Levels, 2, D3D11_SDK_VERSION, &Sd, &Mw.Swap, &P->Dev, nullptr, &P->Dc );
        if ( FAILED( Hr ) ) { std::fprintf( stderr, "tg: couldn't create a D3D11 device (0x%08lx)\n", (unsigned long)Hr ); return; }
        Mw.CreateRtv( P->Dev );
        if ( !P->Dx.Init( P->Dev, P->Dc ) ) { std::fprintf( stderr, "tg: D3D11 renderer init failed\n" ); return; }

        if ( Cfg.Light ) GetTheme().UseColors( Theme::Light() );
        Mw.Ctx = std::make_unique<Context>( P->Dx );
        if ( !Mw.Ctx->LoadDefaultFonts( win32::GetDpiScale( Mw.Wnd ) ) ) return;
        win32::Install( Mw.Wnd, *Mw.Ctx );
        detail::BuildRoot( *Mw.Ctx, Mw.MainArea, Mw.WindowLayer );

        SetWindowLongPtrW( Mw.Wnd, GWLP_USERDATA, (LONG_PTR)&Mw );
        P->Good = true;
    }

    App::~App()
    {
        for ( auto& X : P->Extra )
        {
            detail::PutBack( X->Popped, *X->MainArea );
            P->Destroy( *X );
        }
        P->Extra.clear();
        P->Destroy( P->MainWin ); // widgets + font texture before the device
        P->Dx.Shutdown();
        if ( P->Dc ) P->Dc->Release();
        if ( P->Dev ) P->Dev->Release();
    }

    bool App::Ok() const { return P->Good; }
    Container& App::Main() { return *P->MainWin.MainArea; }
    Context& App::GetContext() { return *P->MainWin.Ctx; }
    Renderer& App::GetRenderer() { return P->Dx; }
    void App::SetContinuous( bool On ) { P->Cfg.Continuous = On; }
    void App::Quit() { if ( P->MainWin.Wnd ) PostMessageW( P->MainWin.Wnd, WM_CLOSE, 0, 0 ); }
    size_t App::WindowCount() const { return 1 + P->Extra.size(); }

    tg::Window& App::Window( std::string Title, Vec2 Pos, Vec2 Size )
    {
        if ( Pos.x < 0 || Pos.y < 0 )
        {
            Pos = { 40.f + 32 * ( P->Cascade % 8 ), 40.f + 32 * ( P->Cascade % 8 ) };
            P->Cascade++;
        }
        return P->MainWin.WindowLayer->Add<tg::Window>( std::move( Title ), Pos, Size );
    }

    OsWindow& App::NewWindow( std::string Title, int Width, int Height )
    {
        auto X = std::make_unique<Win32Window>();
        X->Owner = P.get();
        X->Wnd = P->MakeWindow( Title, Width, Height );
        // one device and renderer for every window; each gets a swapchain and a Context sharing the fonts
        X->Ctx = std::make_unique<Context>( P->Dx, *P->MainWin.Ctx );
        detail::BuildRoot( *X->Ctx, X->MainArea, X->WindowLayer );
        if ( X->Wnd && P->MakeSwapChain( *X ) )
        {
            win32::Install( X->Wnd, *X->Ctx );
            SetWindowLongPtrW( X->Wnd, GWLP_USERDATA, (LONG_PTR)X.get() );
            ShowWindow( X->Wnd, SW_SHOWNORMAL );
        }
        else X->Closing = true; // couldn't open: the next loop iteration cleans up
        P->Extra.push_back( std::move( X ) );
        return *P->Extra.back();
    }

    OsWindow& App::PopOut( Widget& W, std::string Title, int Width, int Height )
    {
        Vec2 S = detail::PopOutSize( W, Width, Height );
        auto& X = static_cast<Win32Window&>( NewWindow( std::move( Title ), int( S.x ), int( S.y ) ) );
        X.Popped = detail::PopOutInto( W, *X.MainArea );
        P->MainWin.Ctx->RequestFrame();
        return X;
    }

    int App::Run()
    {
        if ( !P->Good ) return 1;
        Win32Window& Mw = P->MainWin;
        ShowWindow( Mw.Wnd, SW_SHOWDEFAULT );
        UpdateWindow( Mw.Wnd );
        Context& Ctx = *Mw.Ctx;
        MSG Msg;
        // Keep drawing for a moment after the window appears: some compositors (and Wine) drop
        // presents made while the window is still being mapped, and an idle UI wouldn't redraw.
        const ULONGLONG ShownAt = GetTickCount64();
        while ( !P->Quit )
        {
            if ( P->Cfg.Continuous || GetTickCount64() - ShownAt < 500 ) Ctx.RequestFrame();
            // sleep until any window has work
            double T = Ctx.SecondsUntilNextFrame();
            for ( auto& X : P->Extra ) T = std::min( T, X->Ctx->SecondsUntilNextFrame() );
            if ( T > 0 ) MsgWaitForMultipleObjectsEx( 0, nullptr, T > 0.5 ? 500 : DWORD( T * 1000 ), QS_ALLINPUT, MWMO_INPUTAVAILABLE );
            Ctx.RunPumps();
            for ( auto& X : P->Extra ) X->Ctx->RunPumps();
            while ( PeekMessageW( &Msg, nullptr, 0, 0, PM_REMOVE ) )
            {
                if ( Msg.message == WM_QUIT ) P->Quit = true;
                TranslateMessage( &Msg );
                DispatchMessageW( &Msg );
            }
            if ( P->Quit ) break;
            P->Reap();
            win32::Sync( Mw.Wnd, Ctx );
            if ( Ctx.NeedsFrame() ) P->DrawFrame( Mw );
            for ( auto& X : P->Extra )
            {
                if ( !X->Wnd ) continue;
                win32::Sync( X->Wnd, *X->Ctx );
                if ( X->Ctx->NeedsFrame() ) P->DrawFrame( *X );
            }
        }
        return 0;
    }
}
