// WebView: the platform-independent half. Queues calls until the engine is up, keeps the native
// window glued to the widget's rectangle every frame, and draws a placeholder underneath.
#include "tg/webview.h"
#include "tg/context.h"
#include "tg/draw.h"
#include "tg/font.h"
#include "tg/widgets/popup.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tg
{
    WebView::WebView( std::string Url )
    {
        Layout.Width = Grow();
        Layout.Height = Grow();
        if ( !Url.empty() ) Navigate( Url );
    }

    WebView::~WebView()
    {
        if ( HookOwner ) static_cast<Context*>( HookOwner )->RemoveFrameHook( this );
        B.reset();
    }

    bool WebView::Supported() { return WebBackendAvailable(); }
    bool WebView::Ready() const { return B && B->Ready(); }

    void WebView::Run( std::function<void( WebBackend& )> Fn )
    {
        if ( Ready() ) Fn( *B );
        else Pending.push_back( std::move( Fn ) );
    }

    void WebView::Navigate( const std::string& Url ) { CurrentUrl = Url; Run( [ Url ]( WebBackend& W ) { W.Navigate( Url ); } ); }
    void WebView::SetHtml( const std::string& Html ) { Run( [ Html ]( WebBackend& W ) { W.SetHtml( Html ); } ); }
    void WebView::Eval( const std::string& Js ) { Run( [ Js ]( WebBackend& W ) { W.Eval( Js ); } ); }
    void WebView::Back() { Run( []( WebBackend& W ) { W.Back(); } ); }
    void WebView::Forward() { Run( []( WebBackend& W ) { W.Forward(); } ); }
    void WebView::Reload() { Run( []( WebBackend& W ) { W.Reload(); } ); }

    // C++ string -> JS string literal
    static std::string JsQuote( const std::string& S )
    {
        std::string O = "\"";
        for ( unsigned char C : S )
        {
            switch ( C )
            {
            case '"': O += "\\\""; break;
            case '\\': O += "\\\\"; break;
            case '\n': O += "\\n"; break;
            case '\r': O += "\\r"; break;
            case '\t': O += "\\t"; break;
            case '<': O += "\\x3c"; break; // never "</script>"
            default:
                if ( C < 0x20 ) { char B[ 8 ]; std::snprintf( B, sizeof( B ), "\\x%02x", C ); O += B; }
                else O += char( C );
            }
        }
        // U+2028 / U+2029 are line terminators in older JS engines
        for ( size_t i; ( i = O.find( "\xE2\x80\xA8" ) ) != std::string::npos; ) O.replace( i, 3, "\\u2028" );
        for ( size_t i; ( i = O.find( "\xE2\x80\xA9" ) ) != std::string::npos; ) O.replace( i, 3, "\\u2029" );
        return O + "\"";
    }

    void WebView::Post( const std::string& M ) { Eval( "window.tg&&window.tg._recv(" + JsQuote( M ) + ")" ); }

    const char* WebView::BridgeScript( const char* PostCall )
    {
        // window.tg.post(str) -> C++ OnMessage; C++ Post(str) -> every tg.on(fn) handler.
        static std::string S;
        S = std::string( "(function(){if(window.tg)return;var l=[];window.tg={post:function(m){" ) + PostCall +
            "(String(m));},on:function(f){l.push(f);},_recv:function(m){for(var i=0;i<l.length;i++){try{l[i](m);}catch(e){console.error(e);}}}};})();";
        return S.c_str();
    }

    void WebView::BackendReady()
    {
        auto Q = std::move( Pending );
        Pending.clear();
        for ( auto& Fn : Q ) Fn( *B );
        Redraw();
    }

    void WebView::BackendNavigated( const std::string& Url )
    {
        CurrentUrl = Url;
        if ( OnNavigate ) OnNavigate( Url );
    }

    void WebView::BackendTitle( const std::string& T )
    {
        CurrentTitle = T;
        if ( OnTitle ) OnTitle( T );
    }

    void WebView::BackendError( const std::string& E )
    {
        Error = E;
        if ( OnError ) OnError( E );
        Redraw();
    }

    void WebView::Start()
    {
        Started = true;
        Context* C = GetContext();
        if ( !C ) return;
        if ( !WebBackendAvailable() ) { BackendError( "tg was built without a web view backend for this platform (TG_WEBVIEW=ON)" ); return; }
        if ( !C->NativeWindow ) { BackendError( "no native window to put the web view in (on Linux, X11/XWayland is needed)" ); return; }
        std::string Err;
        B = CreateWebBackend( C->NativeWindow, *this, Err );
        if ( !B ) { BackendError( Err.empty() ? "couldn't create the web view" : Err ); return; }
        HookOwner = C;
        C->AddFrameHook( this, [ this ] { Sync(); } );
        if ( B->Ready() ) BackendReady();
    }

    // After every frame: show the page where the widget was drawn, hide it when it wasn't (other tab,
    // collapsed section, scrolled away) or while a popup overlaps it.
    void WebView::Sync()
    {
        Context* C = GetContext();
        if ( !B || !C ) return;
        bool Visible = DrawnFrame == C->FrameIndex() && Bounds.w > 0 && Bounds.h > 0;
        if ( Visible && HideUnderPopups )
            for ( auto& K : C->Overlay().Children() )
                if ( auto* P = dynamic_cast<Popup*>( K.get() ); P && !P->IsClosed() && !P->Bounds.Intersect( Bounds ).Empty() ) Visible = false;
        float S = C->GetScale();
        auto Px = [ S ]( Rect R ) {
            float X0 = std::round( R.x * S ), Y0 = std::round( R.y * S );
            return Rect{ X0, Y0, std::round( R.Right() * S ) - X0, std::round( R.Bottom() * S ) - Y0 };
        };
        B->Place( Px( Bounds ), Px( DrawnClip ), Visible );
        Shown = Visible;
    }

    Vec2 WebView::Measure( float Avail ) { return { std::min( Avail, 640.f ), 400 }; }

    void WebView::Draw( DrawList& D )
    {
        if ( !Started ) Start();
        Context* C = GetContext();
        if ( C ) DrawnFrame = C->FrameIndex(); // Sync() runs after this frame finishes
        DrawnClip = D.CurrentClip();
        // what shows until the page paints (and if there's no engine)
        const Theme& T = GetTheme();
        D.RectFilled( Bounds, T[ Tok::SurfaceAlt ] );
        const char* Msg = !Error.empty() ? Error.c_str() : !Ready() ? "Loading web view..." : nullptr;
        if ( Msg )
        {
            const Font& F = *T.Body;
            float Wrap = std::max( 40.f, Bounds.w - 32 );
            Vec2 Sz = F.Measure( Msg, Wrap );
            D.Text( F, { std::floor( Bounds.Center().x - Sz.x * 0.5f ), std::floor( Bounds.Center().y - Sz.y * 0.5f ) }, Msg,
                    Error.empty() ? T[ Tok::TextDim ] : T[ Tok::Warning ], Wrap );
        }
    }
}
