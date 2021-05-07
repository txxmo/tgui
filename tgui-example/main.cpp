#include <iostream>
#include "main.h"
#include "render/render.h"
#include "menu/framework/tgui.hpp"

static LPDIRECT3D9              g_pD3D = NULL;
static LPDIRECT3DDEVICE9        g_pd3dDevice = NULL;
static D3DPRESENT_PARAMETERS    g_d3dpp = { };

HWND WindowHWND;
WNDCLASSEX WindowClass;
int Width, Height;
CRender* Draw;

bool Window::CreateDeviceD3D( HWND hWnd ) {
	if ( NULL == ( g_pD3D = Direct3DCreate9( D3D_SDK_VERSION ) ) )
		return FALSE;

	// Create the D3DDevice
	ZeroMemory( &g_d3dpp, sizeof( g_d3dpp ) );
	g_d3dpp.Windowed = TRUE;
	g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
	g_d3dpp.EnableAutoDepthStencil = TRUE;
	g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;           // Present with vsync
	//g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate
	if ( g_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice ) < 0 )
		return false;

	return true;
}

void Window::CleanupDeviceD3D( )
{
	if ( g_pd3dDevice ) { g_pd3dDevice->Release( ); g_pd3dDevice = NULL; }
	if ( g_pD3D ) { g_pD3D->Release( ); g_pD3D = NULL; }
}

void Window::ResetDevice( )
{

	HRESULT hr = g_pd3dDevice->Reset( &g_d3dpp );

}

LRESULT WINAPI Window::WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch ( msg )
	{
	case WM_SIZE:
		if ( g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED )
		{
			g_d3dpp.BackBufferWidth = LOWORD( lParam );
			g_d3dpp.BackBufferHeight = HIWORD( lParam );
			ResetDevice( );
		}
		return 0;
	case WM_SYSCOMMAND:
		if ( ( wParam & 0xfff0 ) == SC_KEYMENU ) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage( 0 );
		return 0;
	}

	TGUI::Input::HandleMouse( msg, wParam, lParam );

	return ::DefWindowProc( hWnd, msg, wParam, lParam );
}

void Window::Do( ) {
	/* Use this to call functions*/

	WNDCLASSEXW wcex;
	wcex.hbrBackground = CreateSolidBrush( RGB( 0, 0, 0 ) ); // black

	// Create application window
	WindowClass = { sizeof( WNDCLASSEX ), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle( NULL ), NULL, LoadCursor( NULL, IDC_ARROW ), NULL, NULL, _T( "TGUI" ), NULL };
	::RegisterClassEx( &WindowClass );
	WindowHWND = ::CreateWindowEx( WS_EX_LAYERED, WindowClass.lpszClassName, "TGUI", WS_POPUP, 0, 0, 1920, 1080, NULL, NULL, WindowClass.hInstance, NULL );

	SetLayeredWindowAttributes( WindowHWND, 0, 255, LWA_ALPHA );
	SetLayeredWindowAttributes( WindowHWND, RGB( 0, 0, 0 ), 0, LWA_COLORKEY );

	// Initialize Direct3D
	if ( !Window::CreateDeviceD3D( WindowHWND ) )
	{
		Window::CleanupDeviceD3D( );
		::UnregisterClass( WindowClass.lpszClassName, WindowClass.hInstance );
		return;
	}

	::ShowWindow( WindowHWND, SW_SHOWDEFAULT );
	::UpdateWindow( WindowHWND );

	MSG msg;
	ZeroMemory( &msg, sizeof( msg ) );
	while ( msg.message != WM_QUIT )
	{
		if ( ::PeekMessage( &msg, NULL, 0U, 0U, PM_REMOVE ) )
		{
			::TranslateMessage( &msg );
			::DispatchMessage( &msg );
			continue;
		}

		g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
		g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
		g_pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, FALSE );
		g_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB( 0, 0, 0, 0 ), 1.0f, 0 );

		if ( g_pd3dDevice->BeginScene( ) >= 0 )
		{
			if ( Draw == NULL )
			{
				Draw = new CRender( g_pd3dDevice );
				D3DXCreateFont( g_pd3dDevice, 14, 0, 400, 4, FALSE, DEFAULT_CHARSET, 1, ANTIALIASED_QUALITY, DEFAULT_PITCH, "Segoe UI", &Draw->F.Segoe );
				D3DXCreateFont( g_pd3dDevice, 14, 0, 800, 4, FALSE, DEFAULT_CHARSET, 1, ANTIALIASED_QUALITY, DEFAULT_PITCH, "Segoe UI", &Draw->F.SegoeBold );
			}

			Draw->InitDevice( );

			TGui->Create( );

			g_pd3dDevice->EndScene( );
		}
		HRESULT result = g_pd3dDevice->Present( NULL, NULL, NULL, NULL );

		// Handle loss of D3D9 device
		if ( result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel( ) == D3DERR_DEVICENOTRESET )
			ResetDevice( );
	}

	Window::CleanupDeviceD3D( );
	::DestroyWindow( WindowHWND );
	::UnregisterClass( WindowClass.lpszClassName, WindowClass.hInstance );
}


int main( )
{
	std::thread( Window::Do ).detach( );

	while ( !GetAsyncKeyState( VK_DELETE ) )
		std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );

	return EXIT_SUCCESS;
}
