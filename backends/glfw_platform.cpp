#include "glfw_platform.h"
#include "tg/context.h"
#include <GLFW/glfw3.h>
#include <algorithm>

namespace tg::glfw
{
    namespace
    {
        Context& Ctx( GLFWwindow* W ) { return *static_cast<Context*>( glfwGetWindowUserPointer( W ) ); }

        // Window coords -> UI units. (Window coords are pixels on Windows/X11, points on macOS.)
        Vec2 ToUI( GLFWwindow* W, double X, double Y )
        {
            int Ww, Wh, Fw, Fh;
            glfwGetWindowSize( W, &Ww, &Wh );
            glfwGetFramebufferSize( W, &Fw, &Fh );
            float Sx = Ww > 0 ? float( Fw ) / Ww : 1.f;
            float Scale = 1.f, Dummy;
            glfwGetWindowContentScale( W, &Scale, &Dummy );
            return { float( X ) * Sx / Scale, float( Y ) * Sx / Scale };
        }

        uint8_t Mods( int M )
        {
            return uint8_t( ( M & GLFW_MOD_SHIFT ? ModShift : 0 ) | ( M & GLFW_MOD_CONTROL ? ModCtrl : 0 ) |
                            ( M & GLFW_MOD_ALT ? ModAlt : 0 ) | ( M & GLFW_MOD_SUPER ? ModSuper : 0 ) );
        }

        Key MapKey( int K )
        {
            switch ( K )
            {
            case GLFW_KEY_TAB: return Key::Tab;
            case GLFW_KEY_ENTER: case GLFW_KEY_KP_ENTER: return Key::Enter;
            case GLFW_KEY_ESCAPE: return Key::Escape;
            case GLFW_KEY_BACKSPACE: return Key::Backspace;
            case GLFW_KEY_DELETE: return Key::Delete;
            case GLFW_KEY_SPACE: return Key::Space;
            case GLFW_KEY_LEFT: return Key::Left;
            case GLFW_KEY_RIGHT: return Key::Right;
            case GLFW_KEY_UP: return Key::Up;
            case GLFW_KEY_DOWN: return Key::Down;
            case GLFW_KEY_HOME: return Key::Home;
            case GLFW_KEY_END: return Key::End;
            case GLFW_KEY_PAGE_UP: return Key::PageUp;
            case GLFW_KEY_PAGE_DOWN: return Key::PageDown;
            case GLFW_KEY_A: return Key::A;
            case GLFW_KEY_C: return Key::C;
            case GLFW_KEY_V: return Key::V;
            case GLFW_KEY_X: return Key::X;
            case GLFW_KEY_Z: return Key::Z;
            default: return Key::Unknown;
            }
        }

        GLFWcursor* Cursors[ 5 ] = {};
        GLFWwindow* ClipboardWindow = nullptr;
    }

    void Install( GLFWwindow* W, Context& C )
    {
        glfwSetWindowUserPointer( W, &C );
        ClipboardWindow = W;

        glfwSetCursorPosCallback( W, []( GLFWwindow* W, double X, double Y ) { Ctx( W ).OnMouseMove( ToUI( W, X, Y ) ); } );
        glfwSetMouseButtonCallback( W, []( GLFWwindow* W, int B, int Action, int ) {
            if ( B > 2 ) return;
            MouseButton Mb = B == GLFW_MOUSE_BUTTON_RIGHT ? MouseButton::Right : B == GLFW_MOUSE_BUTTON_MIDDLE ? MouseButton::Middle : MouseButton::Left;
            Ctx( W ).OnMouseButton( Mb, Action == GLFW_PRESS );
        } );
        glfwSetScrollCallback( W, []( GLFWwindow* W, double X, double Y ) { Ctx( W ).OnMouseWheel( { float( X ), float( Y ) } ); } );
        glfwSetKeyCallback( W, []( GLFWwindow* W, int K, int, int Action, int M ) {
            Ctx( W ).OnKey( MapKey( K ), Action != GLFW_RELEASE, Mods( M ), Action == GLFW_REPEAT );
        } );
        glfwSetCharCallback( W, []( GLFWwindow* W, unsigned int Cp ) { Ctx( W ).OnText( Cp ); } );
        glfwSetWindowRefreshCallback( W, []( GLFWwindow* W ) { Ctx( W ).RequestFrame(); } );

        C.SetClipboard = []( const std::string& S ) { glfwSetClipboardString( ClipboardWindow, S.c_str() ); };
        C.GetClipboard = []() -> std::string { const char* S = glfwGetClipboardString( ClipboardWindow ); return S ? S : ""; };
        C.WakeUp = []() { glfwPostEmptyEvent(); };

        Cursors[ int( Cursor::Arrow ) ] = glfwCreateStandardCursor( GLFW_ARROW_CURSOR );
        Cursors[ int( Cursor::Hand ) ] = glfwCreateStandardCursor( GLFW_HAND_CURSOR );
        Cursors[ int( Cursor::IBeam ) ] = glfwCreateStandardCursor( GLFW_IBEAM_CURSOR );
        Cursors[ int( Cursor::ResizeH ) ] = glfwCreateStandardCursor( GLFW_HRESIZE_CURSOR );
        Cursors[ int( Cursor::ResizeV ) ] = glfwCreateStandardCursor( GLFW_VRESIZE_CURSOR );

        Sync( W, C );
    }

    void Sync( GLFWwindow* W, Context& C )
    {
        int Fw, Fh;
        glfwGetFramebufferSize( W, &Fw, &Fh );
        float Scale = 1.f, Dummy;
        glfwGetWindowContentScale( W, &Scale, &Dummy );
        if ( Scale <= 0 ) Scale = 1;
        C.SetViewport( { Fw / Scale, Fh / Scale }, Scale );

        static Cursor Last = Cursor::Arrow;
        Cursor Want = C.GetCursor();
        if ( Want != Last ) { glfwSetCursor( W, Cursors[ int( Want ) ] ); Last = Want; }
    }

    void WaitForWork( Context& C )
    {
        double T = C.SecondsUntilNextFrame();
        if ( T <= 0 ) glfwPollEvents();
        else glfwWaitEventsTimeout( std::min( T, 0.5 ) );
    }
}
