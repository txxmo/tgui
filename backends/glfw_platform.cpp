#include "glfw_platform.h"
#include "tg/context.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
// Native handle for WebView & co. X11 only when asked for (TG_GLFW_X11): GLFW may be built without it.
#if defined( _WIN32 )
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#elif defined( __APPLE__ )
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#elif defined( TG_GLFW_X11 )
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#endif

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
#include "glfw_keys.inc"
        }

        GLFWcursor* Cursors[ 5 ] = {};
        GLFWwindow* ClipboardWindow = nullptr;
        std::unordered_map<GLFWwindow*, Cursor> LastCursor; // per window (App::NewWindow)
    }

    void Install( GLFWwindow* W, Context& C )
    {
        glfwSetWindowUserPointer( W, &C );
        if ( !ClipboardWindow ) ClipboardWindow = W;
        LastCursor[ W ] = Cursor::Arrow;
#if defined( _WIN32 )
        C.NativeWindow = glfwGetWin32Window( W );
#elif defined( __APPLE__ )
        C.NativeWindow = (void*)glfwGetCocoaWindow( W );
#elif defined( TG_GLFW_X11 )
        C.NativeWindow = (void*)uintptr_t( glfwGetX11Window( W ) ); // 0 when GLFW runs on Wayland
#endif

        glfwSetCursorPosCallback( W, []( GLFWwindow* W, double X, double Y ) { Ctx( W ).OnMouseMove( ToUI( W, X, Y ) ); } );
        glfwSetMouseButtonCallback( W, []( GLFWwindow* W, int B, int Action, int ) {
            if ( B > 4 ) return;
            static const MouseButton Map[] = { MouseButton::Left, MouseButton::Right, MouseButton::Middle, MouseButton::Back, MouseButton::Forward };
            MouseButton Mb = Map[ B ];
            Ctx( W ).OnMouseButton( Mb, Action == GLFW_PRESS );
        } );
        glfwSetScrollCallback( W, []( GLFWwindow* W, double X, double Y ) { Ctx( W ).OnMouseWheel( { float( X ), float( Y ) } ); } );
        glfwSetKeyCallback( W, []( GLFWwindow* W, int K, int, int Action, int M ) {
            Ctx( W ).OnKey( MapKey( K ), Action != GLFW_RELEASE, Mods( M ), Action == GLFW_REPEAT );
        } );
        glfwSetCharCallback( W, []( GLFWwindow* W, unsigned int Cp ) { Ctx( W ).OnText( Cp ); } );
        glfwSetWindowRefreshCallback( W, []( GLFWwindow* W ) { Ctx( W ).RequestFrame(); } );
        // files dropped from the file manager (GLFW reports the drop only, no hover)
        glfwSetDropCallback( W, []( GLFWwindow* W, int N, const char** Paths ) {
            std::vector<std::string> Ps( Paths, Paths + N );
            double X, Y;
            glfwGetCursorPos( W, &X, &Y );
            Ctx( W ).OnFilesDrop( ToUI( W, X, Y ), std::move( Ps ) );
        } );

        C.SetClipboard = []( const std::string& S ) { glfwSetClipboardString( ClipboardWindow, S.c_str() ); };
        C.GetClipboard = []() -> std::string { const char* S = glfwGetClipboardString( ClipboardWindow ); return S ? S : ""; };
        C.WakeUp = []() { glfwPostEmptyEvent(); };

        if ( !Cursors[ 0 ] )
        {
            Cursors[ int( Cursor::Arrow ) ] = glfwCreateStandardCursor( GLFW_ARROW_CURSOR );
            Cursors[ int( Cursor::Hand ) ] = glfwCreateStandardCursor( GLFW_HAND_CURSOR );
            Cursors[ int( Cursor::IBeam ) ] = glfwCreateStandardCursor( GLFW_IBEAM_CURSOR );
            Cursors[ int( Cursor::ResizeH ) ] = glfwCreateStandardCursor( GLFW_HRESIZE_CURSOR );
            Cursors[ int( Cursor::ResizeV ) ] = glfwCreateStandardCursor( GLFW_VRESIZE_CURSOR );
        }

        Sync( W, C );
    }

    void Uninstall( GLFWwindow* W )
    {
        LastCursor.erase( W );
        if ( ClipboardWindow == W ) ClipboardWindow = nullptr;
        glfwSetWindowUserPointer( W, nullptr );
    }

    void Sync( GLFWwindow* W, Context& C )
    {
        int Fw, Fh;
        glfwGetFramebufferSize( W, &Fw, &Fh );
        float Scale = 1.f, Dummy;
        glfwGetWindowContentScale( W, &Scale, &Dummy );
        if ( Scale <= 0 ) Scale = 1;
        C.SetViewport( { Fw / Scale, Fh / Scale }, Scale );

        Cursor& Last = LastCursor[ W ];
        Cursor Want = C.GetCursor();
        if ( Want != Last ) { glfwSetCursor( W, Cursors[ int( Want ) ] ); Last = Want; }
    }

    void WaitForWork( Context& C )
    {
        double T = C.SecondsUntilNextFrame();
        if ( T <= 0 ) glfwPollEvents();
        else glfwWaitEventsTimeout( std::min( T, 0.5 ) );
        C.RunPumps();
    }
}
