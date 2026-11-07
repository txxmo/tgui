// "Bring your own" Win32 + D3D11: your window, your device, your render loop — tg draws on top.
// This is the shape of a game/overlay integration. (If you just want an app, use tg::App instead.)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <cmath>

#include "../common/overlay_menu.h"
#include "d3d11_renderer.h"
#include "win32_platform.h"

static ID3D11Device* Dev;
static ID3D11DeviceContext* Dc;
static IDXGISwapChain* Swap;
static ID3D11RenderTargetView* Rtv;

static void CreateRtv()
{
    ID3D11Texture2D* Back = nullptr;
    Swap->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (void**)&Back );
    Dev->CreateRenderTargetView( Back, nullptr, &Rtv );
    Back->Release();
}

static LRESULT CALLBACK WndProc( HWND H, UINT M, WPARAM W, LPARAM L )
{
    // 1) give tg first look. true = it's tg's input (an overlay would hide it from the game here).
    if ( tg::win32::HandleMessage( H, M, W, L ) && M == WM_SETCURSOR ) return TRUE;

    switch ( M )
    {
    case WM_SIZE:
        if ( Swap && W != SIZE_MINIMIZED )
        {
            if ( Rtv ) { Rtv->Release(); Rtv = nullptr; }
            Swap->ResizeBuffers( 0, LOWORD( L ), HIWORD( L ), DXGI_FORMAT_UNKNOWN, 0 );
            CreateRtv();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage( 0 );
        return 0;
    }
    return DefWindowProcW( H, M, W, L );
}

int main()
{
    tg::win32::EnableDpiAwareness();
    WNDCLASSEXW Wc = { sizeof( Wc ), CS_CLASSDC, WndProc, 0, 0, GetModuleHandleW( nullptr ), nullptr, LoadCursorW( nullptr, MAKEINTRESOURCEW( 32512 ) ) /* IDC_ARROW */, nullptr, nullptr, L"tg_d3d11", nullptr };
    RegisterClassExW( &Wc );
    HWND Wnd = CreateWindowW( Wc.lpszClassName, L"tg - Win32 + D3D11 (bring your own)", WS_OVERLAPPEDWINDOW, 100, 100, 1100, 720, nullptr, nullptr, Wc.hInstance, nullptr );

    DXGI_SWAP_CHAIN_DESC Sd = {};
    Sd.BufferCount = 2;
    Sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    Sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    Sd.OutputWindow = Wnd;
    Sd.SampleDesc.Count = 1;
    Sd.Windowed = TRUE;
    Sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    if ( FAILED( D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &Sd, &Swap, &Dev, nullptr, &Dc ) ) &&
         FAILED( D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &Sd, &Swap, &Dev, nullptr, &Dc ) ) )
        return 1;
    CreateRtv();
    ShowWindow( Wnd, SW_SHOWDEFAULT );

    // 2) tg setup: renderer on your device, context, platform glue on your window
    tg::D3D11Renderer Dx;
    if ( !Dx.Init( Dev, Dc ) ) return 1;
    {
        tg::Context Ctx( Dx );
        Ctx.LoadDefaultFonts( tg::win32::GetDpiScale( Wnd ) );
        tg::win32::Install( Wnd, Ctx );
        OverlayState State;
        BuildOverlayMenu( Ctx, State );

        // 3) your loop. A game renders every frame anyway, so tg just draws on top each frame.
        bool Running = true;
        double T = 0;
        while ( Running )
        {
            MSG Msg;
            while ( PeekMessageW( &Msg, nullptr, 0, 0, PM_REMOVE ) )
            {
                if ( Msg.message == WM_QUIT ) Running = false;
                TranslateMessage( &Msg );
                DispatchMessageW( &Msg );
            }
            if ( !Running ) break;

            // "game" frame: an animated clear color stands in for your scene
            T += 1.0 / 60;
            const float Clear[ 4 ] = { 0.08f + 0.05f * float( std::sin( T ) ), 0.10f, 0.16f + 0.05f * float( std::cos( T * 0.7 ) ), 1 };
            Dc->OMSetRenderTargets( 1, &Rtv, nullptr );
            Dc->ClearRenderTargetView( Rtv, Clear );

            // tg on top
            tg::win32::Sync( Wnd, Ctx );
            Ctx.Frame();
            RECT R;
            GetClientRect( Wnd, &R );
            Ctx.Render( { float( R.right ), float( R.bottom ) } );

            Swap->Present( 1, 0 );
        }
        tg::win32::Shutdown();
    }
    Dx.Shutdown();
    Rtv->Release();
    Swap->Release();
    Dc->Release();
    Dev->Release();
    DestroyWindow( Wnd );
    return 0;
}
