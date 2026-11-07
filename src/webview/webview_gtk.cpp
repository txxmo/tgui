// WebView on Linux: WebKitGTK in a GTK window reparented into the tg window (X11 / XWayland).
//
//   tg window (GLFW, X11)
//    └─ Clip    plain X window at the visible part of the widget: scroll views crop the page
//        └─ GTK toplevel holding the WebKitWebView, at the widget's full size (offset inside Clip)
//
// GTK's event loop is pumped from the app loop (Context::AddPump), so no thread or gtk_main is needed.
#include "tg/context.h"
#include "tg/webview.h"
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <X11/Xlib.h>
#include <cstdint>

namespace tg
{
    static bool GtkUp()
    {
        static bool Ok = [] {
            gdk_set_allowed_backends( "x11" ); // we embed by X window id
            return gtk_init_check( nullptr, nullptr ) != FALSE;
        }();
        return Ok;
    }

    bool WebBackendAvailable() { return true; }

    struct GtkWeb : WebBackend
    {
        WebView& Owner;
        Context* Ctx;
        Display* Dpy = nullptr;
        ::Window Parent = 0, Clip = 0, Top = 0;
        GtkWidget* Win = nullptr;
        WebKitWebView* View = nullptr;
        Rect LastB{ -1, -1, -1, -1 }, LastV{ -1, -1, -1, -1 };
        bool Mapped = false, HasFocus = false;
        uint32_t Clicks = 0;

        GtkWeb( WebView& O, ::Window P ) : Owner( O ), Ctx( O.GetContext() ), Parent( P )
        {
            Dpy = gdk_x11_display_get_xdisplay( gdk_display_get_default() );
            Clip = XCreateSimpleWindow( Dpy, Parent, 0, 0, 1, 1, 0, 0, 0 );

            WebKitUserContentManager* M = webkit_user_content_manager_new();
            webkit_user_content_manager_register_script_message_handler( M, "tg" );
            g_signal_connect( M, "script-message-received::tg", G_CALLBACK( +[]( WebKitUserContentManager*, WebKitJavascriptResult* R, gpointer Self ) {
                char* S = jsc_value_to_string( webkit_javascript_result_get_js_value( R ) );
                static_cast<GtkWeb*>( Self )->Owner.BackendMessage( S ? S : "" );
                g_free( S );
            } ), this );
            WebKitUserScript* Js = webkit_user_script_new( WebView::BridgeScript( "window.webkit.messageHandlers.tg.postMessage" ),
                                                           WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, nullptr, nullptr );
            webkit_user_content_manager_add_script( M, Js );
            webkit_user_script_unref( Js );

            View = WEBKIT_WEB_VIEW( webkit_web_view_new_with_user_content_manager( M ) );
            g_object_unref( M );
            WebKitSettings* St = webkit_web_view_get_settings( View );
            webkit_settings_set_enable_developer_extras( St, Owner.DevTools );
            webkit_settings_set_javascript_can_access_clipboard( St, TRUE );

            g_signal_connect( View, "load-changed", G_CALLBACK( +[]( WebKitWebView* V, WebKitLoadEvent E, gpointer Self ) {
                if ( E == WEBKIT_LOAD_FINISHED )
                {
                    const char* U = webkit_web_view_get_uri( V );
                    static_cast<GtkWeb*>( Self )->Owner.BackendNavigated( U ? U : "" );
                }
            } ), this );
            g_signal_connect( View, "notify::title", G_CALLBACK( +[]( GObject* V, GParamSpec*, gpointer Self ) {
                const char* T = webkit_web_view_get_title( WEBKIT_WEB_VIEW( V ) );
                static_cast<GtkWeb*>( Self )->Owner.BackendTitle( T ? T : "" );
            } ), this );
            g_signal_connect( View, "load-failed", G_CALLBACK( +[]( WebKitWebView*, WebKitLoadEvent, char* Uri, GError* Err, gpointer Self ) -> gboolean {
                if ( Err && Err->code != WEBKIT_NETWORK_ERROR_CANCELLED )
                    static_cast<GtkWeb*>( Self )->Owner.BackendError( std::string( "couldn't load " ) + ( Uri ? Uri : "" ) + ": " + Err->message );
                return FALSE;
            } ), this );
            // clicking the page gives it the keyboard (a reparented toplevel doesn't get focus from the WM)
            g_signal_connect( View, "button-press-event", G_CALLBACK( +[]( GtkWidget*, GdkEvent*, gpointer Self ) -> gboolean {
                auto* W = static_cast<GtkWeb*>( Self );
                XSetInputFocus( W->Dpy, W->Top, RevertToParent, CurrentTime );
                W->HasFocus = true;
                return FALSE;
            } ), this );

            Win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
            gtk_window_set_decorated( GTK_WINDOW( Win ), FALSE );
            gtk_container_add( GTK_CONTAINER( Win ), GTK_WIDGET( View ) );
            gtk_widget_realize( Win );
            Top = gdk_x11_window_get_xid( gtk_widget_get_window( Win ) );
            XReparentWindow( Dpy, Top, Clip, 0, 0 );
            gtk_widget_show_all( Win );
            XFlush( Dpy );
            if ( Ctx )
            {
                Clicks = Ctx->KeyPressCount( Key::Mouse1 );
                Ctx->AddPump( this, [] { while ( gtk_events_pending() ) gtk_main_iteration_do( FALSE ); } );
            }
        }

        ~GtkWeb() override
        {
            if ( Ctx ) Ctx->RemovePump( this );
            if ( Win ) gtk_widget_destroy( Win );
            if ( Clip ) XDestroyWindow( Dpy, Clip );
            XFlush( Dpy );
            while ( gtk_events_pending() ) gtk_main_iteration_do( FALSE );
        }

        bool Ready() const override { return true; }
        void Navigate( const std::string& U ) override { webkit_web_view_load_uri( View, U.c_str() ); }
        void SetHtml( const std::string& H ) override { webkit_web_view_load_html( View, H.c_str(), "about:blank" ); }
        void Eval( const std::string& J ) override
        {
#if WEBKIT_CHECK_VERSION( 2, 40, 0 )
            webkit_web_view_evaluate_javascript( View, J.c_str(), gssize( J.size() ), nullptr, nullptr, nullptr, nullptr, nullptr );
#else
            webkit_web_view_run_javascript( View, J.c_str(), nullptr, nullptr, nullptr );
#endif
        }
        void Back() override { webkit_web_view_go_back( View ); }
        void Forward() override { webkit_web_view_go_forward( View ); }
        void Reload() override { webkit_web_view_reload( View ); }

        void Place( Rect B, Rect C, bool Visible ) override
        {
            // a click in tg (not on the page: those go to GTK) takes the keyboard back
            if ( Ctx && Ctx->KeyPressCount( Key::Mouse1 ) != Clicks )
            {
                Clicks = Ctx->KeyPressCount( Key::Mouse1 );
                if ( HasFocus ) { XSetInputFocus( Dpy, Parent, RevertToParent, CurrentTime ); HasFocus = false; }
            }
            Rect V = B.Intersect( C );
            if ( !Visible || V.Empty() )
            {
                if ( Mapped ) { XUnmapWindow( Dpy, Clip ); Mapped = false; XFlush( Dpy ); }
                return;
            }
            bool Changed = false;
            if ( V.x != LastV.x || V.y != LastV.y || V.w != LastV.w || V.h != LastV.h )
            {
                XMoveResizeWindow( Dpy, Clip, int( V.x ), int( V.y ), unsigned( V.w ), unsigned( V.h ) );
                LastV = V;
                Changed = true;
            }
            if ( B.x - V.x != LastB.x || B.y - V.y != LastB.y || B.w != LastB.w || B.h != LastB.h )
            {
                gtk_window_resize( GTK_WINDOW( Win ), int( B.w ), int( B.h ) );
                XMoveResizeWindow( Dpy, Top, int( B.x - V.x ), int( B.y - V.y ), unsigned( B.w ), unsigned( B.h ) );
                LastB = { B.x - V.x, B.y - V.y, B.w, B.h };
                Changed = true;
            }
            if ( !Mapped ) { XMapWindow( Dpy, Clip ); Mapped = true; Changed = true; }
            if ( Changed ) XFlush( Dpy );
        }
    };

    std::unique_ptr<WebBackend> CreateWebBackend( void* Native, WebView& Owner, std::string& Error )
    {
        if ( !GtkUp() ) { Error = "GTK couldn't start (no X11 display?)"; return nullptr; }
        return std::make_unique<GtkWeb>( Owner, ::Window( uintptr_t( Native ) ) );
    }
}
