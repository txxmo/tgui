#include "../tgui.hpp"

using namespace TGUI;

void Input::HandleKeys( )
{
    for ( int i = 0; i < 256; i++ ) {
        PrevKeyState[ i ] = KeyState[ i ];
        KeyState[ i ] = GetAsyncKeyState( i );
    }
}

void Input::HandleMouse( UINT Msg, WPARAM WParam, LPARAM LParam )
{
    if ( Msg == WM_KEYDOWN )
    {
        if ( WParam == VK_INSERT )
            Menu::IsOpen = !Menu::IsOpen;
    }

    if ( Msg == WM_MOUSEMOVE )
    {
       MousePos.x = LOWORD( LParam );
        MousePos.y = HIWORD( LParam );
    }

    if ( Msg == WM_MOUSEWHEEL )
    {
        MouseWheel = GET_WHEEL_DELTA_WPARAM( WParam ) / WHEEL_DELTA;
    }
}

bool Input::IsHovering( Vector2D Pos, Vector2D Area )
{
    return MousePos.x > Pos.x && MousePos.y > Pos.y &&
        MousePos.x < Pos.x + Area.x && MousePos.y < Pos.y + Area.y;
}

bool Input::KeyPress( const int Key )
{
    return KeyState[ Key ] && !PrevKeyState[ Key ];
}

bool Input::KeyDown( const int Key )
{
    return KeyState[ Key ];
}

bool Input::KeyRelease( const int Key )
{
    return KeyState[ Key ] && PrevKeyState[ Key ];
}