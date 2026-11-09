// tg/webview.h — a web page inside your tg window. Link tg::webview (CMake -DTG_WEBVIEW=ON).
//
//   auto& Web = Parent.Add<tg::WebView>( "https://grafana.example.com" );
//   Web.SetHtml( "<h1>hi</h1><button onclick=\"tg.post('clicked')\">go</button>" );
//   Web.OnMessage = []( const std::string& M ) { ... };   // from JS: tg.post( "..." )
//   Web.Post( "{\"cpu\": 42}" );                           // to JS:   tg.on( m => ... )
//   Web.Eval( "document.title" );
//
// It uses the browser engine the OS already has: WebView2 (Edge) on Windows, WebKitGTK on Linux,
// WKWebView on macOS. Nothing is bundled, so an app stays small.
//
// The page is a native child window placed over the widget's rectangle. That means tg can't draw on top
// of it: popups (menus, combos) that overlap it hide it while they're open, and tg::Windows dragged over
// it end up underneath. Give it its own area (a tab, a splitter pane, a panel) and it behaves like any widget.
#pragma once
#include "widget.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tg
{
    struct WebBackend;

    class WebView : public Widget
    {
    public:
        explicit WebView( std::string Url = {} );
        ~WebView() override;

        void Navigate( const std::string& Url );
        void SetHtml( const std::string& Html );
        void Eval( const std::string& Js );            // fire and forget; reply with tg.post() if you need a result
        void Post( const std::string& Message );       // delivered to tg.on( fn ) handlers in the page
        void Back();
        void Forward();
        void Reload();

        std::function<void( const std::string& )> OnMessage;  // tg.post( str ) from the page
        std::function<void( const std::string& Url )> OnNavigate; // a page finished loading
        std::function<void( const std::string& )> OnTitle;
        std::function<void( const std::string& )> OnError;    // engine missing / failed to start

        const std::string& Url() const { return CurrentUrl; }
        const std::string& Title() const { return CurrentTitle; }
        bool Ready() const;          // the engine is up (creation is asynchronous on Windows)
        // Start the engine now instead of on first draw: a page that works while hidden (a call engine,
        // a background sync). Needs the widget in a Context's tree.
        void Preload() { if ( !Started ) Start(); }
        static bool Supported();     // built with a backend for this OS

        // Windows: where WebView2 keeps cookies & cache. Default: %LOCALAPPDATA%\<exe name>\WebView2.
        std::string DataDir;
        // The page asks for "microphone", "camera", "notifications", "geolocation", "clipboard" or "other".
        // Return true to allow. Not set: the engine's default (WebView2 asks the user; WebKitGTK denies).
        std::function<bool( const std::string& Kind, const std::string& Origin )> OnPermission;
        // Extra engine switches for every WebView in this process; set before the first one starts.
        // WebView2: Chromium flags, e.g. "--autoplay-policy=no-user-gesture-required". (Ignored elsewhere.)
        static inline std::string BrowserArguments;
        bool DevTools = false;       // allow right-click > Inspect
        bool HideUnderPopups = true; // hide the page while a tg popup overlaps it (it would cover the popup)

        Vec2 Measure( float AvailWidth ) override;
        void Draw( DrawList& D ) override;

        // ---- called by the backends (UI thread)
        void BackendReady();
        void BackendMessage( const std::string& M ) { if ( OnMessage ) OnMessage( M ); }
        void BackendNavigated( const std::string& Url );
        void BackendTitle( const std::string& T );
        void BackendError( const std::string& E );
        // 1 allow, 0 deny, -1 no handler (engine default)
        int BackendPermission( const std::string& Kind, const std::string& Origin ) const
        {
            return OnPermission ? ( OnPermission( Kind, Origin ) ? 1 : 0 ) : -1;
        }
        static const char* BridgeScript( const char* PostCall ); // defines window.tg in every page

    private:
        std::unique_ptr<WebBackend> B;
        std::vector<std::function<void( WebBackend& )>> Pending; // calls made before the engine was ready
        std::string CurrentUrl, CurrentTitle, Error;
        uint64_t DrawnFrame = ~0ull;
        Rect DrawnClip;
        bool Started = false, Shown = false;
        void* HookOwner = nullptr; // the Context we registered our frame hook with
        void Run( std::function<void( WebBackend& )> Fn );
        void Start();
        void Sync();
    };

    // ---- backend interface (src/webview/webview_<os>.cpp)
    struct WebBackend
    {
        virtual ~WebBackend() = default;
        virtual bool Ready() const = 0;
        virtual void Navigate( const std::string& Url ) = 0;
        virtual void SetHtml( const std::string& Html ) = 0;
        virtual void Eval( const std::string& Js ) = 0;
        virtual void Back() = 0;
        virtual void Forward() = 0;
        virtual void Reload() = 0;
        // Bounds and Clip in pixels of the native window's client area. Show the part of Bounds inside Clip.
        virtual void Place( Rect Bounds, Rect Clip, bool Visible ) = 0;
    };
    std::unique_ptr<WebBackend> CreateWebBackend( void* NativeWindow, WebView& Owner, std::string& Error );
    bool WebBackendAvailable();
}
