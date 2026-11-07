// tg::App on Win32 + Direct3D 11 — the native Windows backend (no GLFW needed).
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>

#include "tg/app.h"
#include "d3d11_renderer.h"
#include "win32_platform.h"
#include <cstdio>
#include <string>

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

namespace tg
{
    struct App::Impl
    {
        AppConfig Cfg;
        HWND Wnd = nullptr;
        ID3D11Device* Dev = nullptr;
        ID3D11DeviceContext* Dc = nullptr;
        IDXGISwapChain* Swap = nullptr;
        ID3D11RenderTargetView* Rtv = nullptr;
        D3D11Renderer Dx;
        std::unique_ptr<Context> Ctx;
        Container* MainArea = nullptr;
        Widget* WindowLayer = nullptr;
        int Cascade = 0;
        bool Good = false, Quit = false;
        App* Owner = nullptr;

        void CreateRtv()
        {
            ID3D11Texture2D* Back = nullptr;
            if ( SUCCEEDED( Swap->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (void**)&Back ) ) )
            {
                Dev->CreateRenderTargetView( Back, nullptr, &Rtv );
                Back->Release();
            }
        }

        void Resize( UINT W, UINT H )
        {
            if ( !Swap || W == 0 || H == 0 ) return;
            if ( Rtv ) { Rtv->Release(); Rtv = nullptr; }
            Swap->ResizeBuffers( 0, W, H, DXGI_FORMAT_UNKNOWN, 0 );
            CreateRtv();
            if ( Ctx ) Ctx->RequestFrame();
        }

        void DrawFrame()
        {
            if ( !Ctx || !Rtv ) return;
            win32::Sync( Wnd, *Ctx );
            if ( Owner->OnUpdate ) Owner->OnUpdate();
            Ctx->Frame();
            RECT R;
            GetClientRect( Wnd, &R );
            Color Bg = GetTheme()[ Tok::Background ];
            const float Clear[ 4 ] = { Bg.r / 255.f, Bg.g / 255.f, Bg.b / 255.f, 1.f };
            Dc->OMSetRenderTargets( 1, &Rtv, nullptr );
            Dc->ClearRenderTargetView( Rtv, Clear );
            Ctx->Render( { float( R.right - R.left ), float( R.bottom - R.top ) } );
            Swap->Present( Cfg.VSync ? 1 : 0, 0 );
        }

        static LRESULT CALLBACK WndProc( HWND H, UINT M, WPARAM W, LPARAM L )
        {
            auto* Self = (Impl*)GetWindowLongPtrW( H, GWLP_USERDATA );
            // We own this window: tg sees every message, Windows still gets them (Alt+F4 etc.).
            bool Used = Self && Self->Ctx && win32::HandleMessage( H, M, W, L );
            if ( Used && M == WM_SETCURSOR ) return TRUE;
            switch ( M )
            {
            case WM_SIZE:
                if ( Self && W != SIZE_MINIMIZED )
                {
                    Self->Resize( LOWORD( L ), HIWORD( L ) );
                    Self->DrawFrame(); // keep painting during the modal resize loop
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
                if ( Self ) Self->DrawFrame();
                return 0;
            }
            case WM_ERASEBKGND:
                return 1;
            case WM_SHOWWINDOW:
            case WM_ACTIVATE:
            case WM_WINDOWPOSCHANGED:
                if ( Self && Self->Ctx ) Self->Ctx->RequestFrame();
                break;
            case WM_CLOSE:
                if ( Self ) Self->Quit = true;
                DestroyWindow( H );
                return 0;
            case WM_DESTROY:
                PostQuitMessage( 0 );
                return 0;
            }
            return DefWindowProcW( H, M, W, L );
        }
    };

    static std::wstring Widen( const std::string& S )
    {
        int N = MultiByteToWideChar( CP_UTF8, 0, S.c_str(), -1, nullptr, 0 );
        std::wstring W( size_t( N > 0 ? N - 1 : 0 ), L'\0' );
        if ( N > 1 ) MultiByteToWideChar( CP_UTF8, 0, S.c_str(), -1, W.data(), N );
        return W;
    }

    App::App( const std::string& Title, int W, int H ) : App( AppConfig{ Title, W, H } ) {}

    App::App( const AppConfig& Cfg ) : P( std::make_unique<Impl>() )
    {
        P->Cfg = Cfg;
        P->Owner = this;
        win32::EnableDpiAwareness();

        WNDCLASSEXW Wc = { sizeof( Wc ) };
        Wc.style = CS_HREDRAW | CS_VREDRAW;
        Wc.lpfnWndProc = Impl::WndProc;
        Wc.hInstance = GetModuleHandleW( nullptr );
        Wc.hCursor = LoadCursorW( nullptr, MAKEINTRESOURCEW( 32512 ) ) /* IDC_ARROW */;
        Wc.lpszClassName = L"tg_app";
        RegisterClassExW( &Wc );

        // Size the client area in UI units at the primary monitor's DPI.
        HDC Screen = GetDC( nullptr );
        float Scale = GetDeviceCaps( Screen, LOGPIXELSX ) / 96.f;
        ReleaseDC( nullptr, Screen );
        RECT R = { 0, 0, LONG( Cfg.Width * Scale ), LONG( Cfg.Height * Scale ) };
        AdjustWindowRectEx( &R, WS_OVERLAPPEDWINDOW, FALSE, 0 );
        P->Wnd = CreateWindowExW( 0, Wc.lpszClassName, Widen( Cfg.Title ).c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                  R.right - R.left, R.bottom - R.top, nullptr, nullptr, Wc.hInstance, nullptr );
        if ( !P->Wnd ) { std::fprintf( stderr, "tg: CreateWindow failed\n" ); return; }

        DXGI_SWAP_CHAIN_DESC Sd = {};
        Sd.BufferCount = 2;
        Sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        Sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        Sd.OutputWindow = P->Wnd;
        Sd.SampleDesc.Count = 1;
        Sd.Windowed = TRUE;
        Sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        const D3D_FEATURE_LEVEL Levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        HRESULT Hr = D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, Levels, 2, D3D11_SDK_VERSION, &Sd, &P->Swap, &P->Dev, nullptr, &P->Dc );
        if ( FAILED( Hr ) ) // no GPU driver (VMs, CI): fall back to the WARP software rasterizer
            Hr = D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, Levels, 2, D3D11_SDK_VERSION, &Sd, &P->Swap, &P->Dev, nullptr, &P->Dc );
        if ( FAILED( Hr ) ) { std::fprintf( stderr, "tg: couldn't create a D3D11 device (0x%08lx)\n", (unsigned long)Hr ); return; }
        P->CreateRtv();
        if ( !P->Dx.Init( P->Dev, P->Dc ) ) { std::fprintf( stderr, "tg: D3D11 renderer init failed\n" ); return; }

        if ( Cfg.Light ) GetTheme().UseColors( Theme::Light() );
        P->Ctx = std::make_unique<Context>( P->Dx );
        if ( !P->Ctx->LoadDefaultFonts( win32::GetDpiScale( P->Wnd ) ) ) return;
        win32::Install( P->Wnd, *P->Ctx );

        Widget& Root = P->Ctx->Root();
        Root.Layout.Direction = Dir::Stack;
        P->MainArea = &Root.Add<Container>( Dir::Column, 0 );
        P->WindowLayer = &Root.Add<Widget>();
        P->WindowLayer->Layout.Direction = Dir::Free;

        SetWindowLongPtrW( P->Wnd, GWLP_USERDATA, (LONG_PTR)P.get() );
        P->Good = true;
    }

    App::~App()
    {
        if ( P->Wnd ) SetWindowLongPtrW( P->Wnd, GWLP_USERDATA, 0 );
        win32::Shutdown();
        P->Ctx.reset(); // widgets + font texture before the device
        P->Dx.Shutdown();
        if ( P->Rtv ) P->Rtv->Release();
        if ( P->Swap ) P->Swap->Release();
        if ( P->Dc ) P->Dc->Release();
        if ( P->Dev ) P->Dev->Release();
        if ( P->Wnd && IsWindow( P->Wnd ) ) DestroyWindow( P->Wnd );
    }

    bool App::Ok() const { return P->Good; }
    Container& App::Main() { return *P->MainArea; }
    Context& App::GetContext() { return *P->Ctx; }
    Renderer& App::GetRenderer() { return P->Dx; }
    void App::SetContinuous( bool On ) { P->Cfg.Continuous = On; }
    void App::Quit() { if ( P->Wnd ) PostMessageW( P->Wnd, WM_CLOSE, 0, 0 ); }

    tg::Window& App::Window( std::string Title, Vec2 Pos, Vec2 Size )
    {
        if ( Pos.x < 0 || Pos.y < 0 )
        {
            Pos = { 40.f + 32 * ( P->Cascade % 8 ), 40.f + 32 * ( P->Cascade % 8 ) };
            P->Cascade++;
        }
        return P->WindowLayer->Add<tg::Window>( std::move( Title ), Pos, Size );
    }

    int App::Run()
    {
        if ( !P->Good ) return 1;
        ShowWindow( P->Wnd, SW_SHOWDEFAULT );
        UpdateWindow( P->Wnd );
        Context& Ctx = *P->Ctx;
        MSG Msg;
        // Keep drawing for a moment after the window appears: some compositors (and Wine) drop
        // presents made while the window is still being mapped, and an idle UI wouldn't redraw.
        const ULONGLONG ShownAt = GetTickCount64();
        while ( !P->Quit )
        {
            if ( P->Cfg.Continuous || GetTickCount64() - ShownAt < 500 ) Ctx.RequestFrame();
            win32::WaitForWork( Ctx ); // sleeps when nothing changes
            while ( PeekMessageW( &Msg, nullptr, 0, 0, PM_REMOVE ) )
            {
                if ( Msg.message == WM_QUIT ) P->Quit = true;
                TranslateMessage( &Msg );
                DispatchMessageW( &Msg );
            }
            if ( P->Quit ) break;
            win32::Sync( P->Wnd, Ctx );
            if ( Ctx.NeedsFrame() ) P->DrawFrame();
        }
        return 0;
    }
}
