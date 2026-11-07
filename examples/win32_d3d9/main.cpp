// "Bring your own" Win32 + Direct3D 9 — the same setup the old tgui used (DX9, draw in EndScene).
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d9.h>
#include <cmath>

#include "../common/overlay_menu.h"
#include "d3d9_renderer.h"
#include "win32_platform.h"

static IDirect3D9* D3d;
static IDirect3DDevice9* Dev;
static D3DPRESENT_PARAMETERS Pp;
static tg::D3D9Renderer* Dx;
static UINT ResizeW, ResizeH;

static LRESULT CALLBACK WndProc( HWND H, UINT M, WPARAM W, LPARAM L )
{
    if ( tg::win32::HandleMessage( H, M, W, L ) && M == WM_SETCURSOR ) return TRUE;
    switch ( M )
    {
    case WM_SIZE:
        if ( W != SIZE_MINIMIZED ) { ResizeW = LOWORD( L ); ResizeH = HIWORD( L ); }
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
    WNDCLASSEXW Wc = { sizeof( Wc ), CS_CLASSDC, WndProc, 0, 0, GetModuleHandleW( nullptr ), nullptr, LoadCursorW( nullptr, IDC_ARROW ), nullptr, nullptr, L"tg_d3d9", nullptr };
    RegisterClassExW( &Wc );
    HWND Wnd = CreateWindowW( Wc.lpszClassName, L"tg - Win32 + D3D9 (bring your own)", WS_OVERLAPPEDWINDOW, 100, 100, 1100, 720, nullptr, nullptr, Wc.hInstance, nullptr );

    D3d = Direct3DCreate9( D3D_SDK_VERSION );
    if ( !D3d ) return 1;
    Pp = {};
    Pp.Windowed = TRUE;
    Pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    Pp.BackBufferFormat = D3DFMT_UNKNOWN;
    Pp.EnableAutoDepthStencil = TRUE;
    Pp.AutoDepthStencilFormat = D3DFMT_D16;
    Pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    if ( FAILED( D3d->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Wnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &Pp, &Dev ) ) &&
         FAILED( D3d->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Wnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &Pp, &Dev ) ) )
        return 1;
    ShowWindow( Wnd, SW_SHOWDEFAULT );

    tg::D3D9Renderer Renderer;
    Dx = &Renderer;
    Renderer.Init( Dev );
    {
        tg::Context Ctx( Renderer );
        Ctx.LoadDefaultFonts( tg::win32::GetDpiScale( Wnd ) );
        tg::win32::Install( Wnd, Ctx );
        OverlayState State;
        BuildOverlayMenu( Ctx, State );

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

            // DX9 device reset on resize: tg's buffers go first, then come back
            if ( ResizeW && ResizeH )
            {
                Pp.BackBufferWidth = ResizeW;
                Pp.BackBufferHeight = ResizeH;
                ResizeW = ResizeH = 0;
                Renderer.InvalidateDeviceObjects();
                Dev->Reset( &Pp );
                Renderer.CreateDeviceObjects();
            }

            T += 1.0 / 60;
            int R = int( 20 + 12 * std::sin( T ) ), B = int( 40 + 12 * std::cos( T * 0.7 ) );
            Dev->Clear( 0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB( R, 26, B ), 1.f, 0 );
            if ( SUCCEEDED( Dev->BeginScene() ) )
            {
                // ... the game's scene would be drawn here ...
                tg::win32::Sync( Wnd, Ctx );
                Ctx.Frame();
                RECT Rc;
                GetClientRect( Wnd, &Rc );
                Ctx.Render( { float( Rc.right ), float( Rc.bottom ) } );
                Dev->EndScene();
            }
            Dev->Present( nullptr, nullptr, nullptr, nullptr );
        }
        tg::win32::Shutdown();
    }
    Renderer.Shutdown();
    Dev->Release();
    D3d->Release();
    DestroyWindow( Wnd );
    return 0;
}
