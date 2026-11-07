// WebView on Windows: Microsoft Edge WebView2. The runtime ships with Windows 11 and current Windows 10;
// elsewhere: https://developer.microsoft.com/microsoft-edge/webview2/ (Evergreen bootstrapper).
//
//   tg window (HWND)
//    └─ Clip    child HWND at the visible part of the widget: scroll views crop the page
//        └─ WebView2 controller at the widget's full size (offset inside Clip)
//
// MSVC links WebView2LoaderStatic.lib (nothing to ship). MinGW loads WebView2Loader.dll at runtime,
// which CMake copies next to your exe.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "tg/context.h"
#include "tg/webview.h"
#include <windows.h>
#include <objbase.h>
#include "WebView2.h"
#include <atomic>
#include <cstdio>
#include <functional>

namespace tg
{
    bool WebBackendAvailable() { return true; }

    static std::wstring Wide( const std::string& S )
    {
        int N = MultiByteToWideChar( CP_UTF8, 0, S.c_str(), int( S.size() ), nullptr, 0 );
        std::wstring W( size_t( N ), L'\0' );
        if ( N ) MultiByteToWideChar( CP_UTF8, 0, S.c_str(), int( S.size() ), W.data(), N );
        return W;
    }
    static std::string Narrow( const wchar_t* W )
    {
        if ( !W ) return {};
        int N = WideCharToMultiByte( CP_UTF8, 0, W, -1, nullptr, 0, nullptr, nullptr );
        std::string S( size_t( N > 0 ? N - 1 : 0 ), '\0' );
        if ( N > 1 ) WideCharToMultiByte( CP_UTF8, 0, W, -1, S.data(), N, nullptr, nullptr );
        return S;
    }
    static std::string TakeString( LPWSTR W ) // WebView2 strings are CoTaskMem-allocated
    {
        std::string S = Narrow( W );
        if ( W ) CoTaskMemFree( W );
        return S;
    }

    // One small COM object per callback type: Invoke forwards to a std::function.
    template<typename I, typename... A>
    struct Callback final : I
    {
        std::function<HRESULT( A... )> Fn;
        std::atomic<ULONG> Refs{ 1 };
        explicit Callback( std::function<HRESULT( A... )> F ) : Fn( std::move( F ) ) {}
        HRESULT STDMETHODCALLTYPE QueryInterface( REFIID, void** P ) override { *P = this; AddRef(); return S_OK; }
        ULONG STDMETHODCALLTYPE AddRef() override { return ++Refs; }
        ULONG STDMETHODCALLTYPE Release() override
        {
            ULONG N = --Refs;
            if ( !N ) delete this;
            return N;
        }
        HRESULT STDMETHODCALLTYPE Invoke( A... Args ) override { return Fn( Args... ); }
    };
    template<typename I, typename... A, typename F> static I* MakeCallback( F&& Fn )
    {
        return new Callback<I, A...>( std::function<HRESULT( A... )>( std::forward<F>( Fn ) ) );
    }

    using CreateEnvFn = HRESULT( STDAPICALLTYPE* )( PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions*,
                                                    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* );
    static CreateEnvFn Loader( std::string& Error )
    {
#ifdef TG_WEBVIEW2_STATIC
        (void)Error;
        return &CreateCoreWebView2EnvironmentWithOptions;
#else
        static HMODULE M = LoadLibraryW( L"WebView2Loader.dll" );
        if ( !M ) { Error = "WebView2Loader.dll not found next to the program"; return nullptr; }
        return (CreateEnvFn)(void*)GetProcAddress( M, "CreateCoreWebView2EnvironmentWithOptions" );
#endif
    }

    struct Win32Web : WebBackend
    {
        WebView& Owner;
        Context* Ctx;
        HWND Parent, Clip = nullptr;
        ICoreWebView2Controller* Ctl = nullptr;
        ICoreWebView2* Wv = nullptr;
        std::shared_ptr<int> Alive = std::make_shared<int>( 0 ); // async callbacks check this
        Rect LastB{ -1, -1, -1, -1 };
        bool Visible = false;
        uint32_t Clicks = 0;

        Win32Web( WebView& O, HWND P ) : Owner( O ), Ctx( O.GetContext() ), Parent( P )
        {
            // the parent must not paint over its child windows
            SetWindowLongPtrW( Parent, GWL_STYLE, GetWindowLongPtrW( Parent, GWL_STYLE ) | WS_CLIPCHILDREN );
            static ATOM Cls = [] {
                WNDCLASSW C{};
                C.lpfnWndProc = DefWindowProcW;
                C.hInstance = GetModuleHandleW( nullptr );
                C.lpszClassName = L"tgWebViewClip";
                return RegisterClassW( &C );
            }();
            (void)Cls;
            Clip = CreateWindowExW( 0, L"tgWebViewClip", L"", WS_CHILD | WS_CLIPCHILDREN, 0, 0, 1, 1, Parent, nullptr, GetModuleHandleW( nullptr ), nullptr );
            if ( Ctx ) Clicks = Ctx->KeyPressCount( Key::Mouse1 );
        }

        ~Win32Web() override
        {
            Alive.reset();
            if ( Ctl ) { Ctl->Close(); Ctl->Release(); }
            if ( Wv ) Wv->Release();
            if ( Clip ) DestroyWindow( Clip );
        }

        bool Start( CreateEnvFn Create, std::string& Error )
        {
            CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED ); // fine if the app already did
            std::wstring Dir = Wide( Owner.DataDir );
            if ( Dir.empty() )
            {
                // %LOCALAPPDATA%\<exe>\WebView2 — the default (next to the exe) fails under Program Files
                wchar_t Exe[ MAX_PATH ] = {}, App[ MAX_PATH ] = {};
                GetModuleFileNameW( nullptr, Exe, MAX_PATH );
                std::wstring Name = Exe;
                Name = Name.substr( Name.find_last_of( L"\\/" ) + 1 );
                Name = Name.substr( 0, Name.find_last_of( L'.' ) );
                if ( GetEnvironmentVariableW( L"LOCALAPPDATA", App, MAX_PATH ) ) Dir = std::wstring( App ) + L"\\" + Name + L"\\WebView2";
            }
            std::weak_ptr<int> Weak = Alive;
            auto* OnEnv = MakeCallback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, HRESULT, ICoreWebView2Environment*>(
                [ this, Weak ]( HRESULT R, ICoreWebView2Environment* Env ) -> HRESULT {
                    if ( Weak.expired() ) return S_OK;
                    if ( FAILED( R ) || !Env ) { Fail( R ); return S_OK; }
                    auto* OnCtl = MakeCallback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, HRESULT, ICoreWebView2Controller*>(
                        [ this, Weak ]( HRESULT R2, ICoreWebView2Controller* C ) -> HRESULT {
                            if ( Weak.expired() ) return S_OK;
                            if ( FAILED( R2 ) || !C ) { Fail( R2 ); return S_OK; }
                            Ctl = C;
                            Ctl->AddRef();
                            Ctl->get_CoreWebView2( &Wv );
                            Setup();
                            return S_OK;
                        } );
                    Env->CreateCoreWebView2Controller( Clip, OnCtl );
                    OnCtl->Release();
                    return S_OK;
                } );
            HRESULT R = Create( nullptr, Dir.empty() ? nullptr : Dir.c_str(), nullptr, OnEnv );
            OnEnv->Release();
            if ( FAILED( R ) ) { Error = Describe( R ); return false; }
            return true;
        }

        static std::string Describe( HRESULT R )
        {
            if ( R == HRESULT_FROM_WIN32( ERROR_FILE_NOT_FOUND ) )
                return "The Microsoft Edge WebView2 Runtime isn't installed. Get it from https://developer.microsoft.com/microsoft-edge/webview2/";
            char B[ 64 ];
            std::snprintf( B, sizeof( B ), "WebView2 failed to start (0x%08lX)", (unsigned long)R );
            return B;
        }
        void Fail( HRESULT R ) { Owner.BackendError( Describe( R ) ); }

        void Setup()
        {
            if ( !Wv ) return;
            ICoreWebView2Settings* S = nullptr;
            if ( SUCCEEDED( Wv->get_Settings( &S ) ) && S )
            {
                S->put_AreDevToolsEnabled( Owner.DevTools );
                S->put_IsStatusBarEnabled( FALSE );
                S->Release();
            }
            Wv->AddScriptToExecuteOnDocumentCreated( Wide( WebView::BridgeScript( "window.chrome.webview.postMessage" ) ).c_str(), nullptr );
            EventRegistrationToken T;
            auto* OnMsg = MakeCallback<ICoreWebView2WebMessageReceivedEventHandler, ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs*>(
                [ this ]( ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* A ) -> HRESULT {
                    LPWSTR M = nullptr;
                    if ( SUCCEEDED( A->TryGetWebMessageAsString( &M ) ) ) Owner.BackendMessage( TakeString( M ) );
                    return S_OK;
                } );
            Wv->add_WebMessageReceived( OnMsg, &T );
            OnMsg->Release();
            auto* OnNav = MakeCallback<ICoreWebView2NavigationCompletedEventHandler, ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*>(
                [ this ]( ICoreWebView2* W, ICoreWebView2NavigationCompletedEventArgs* A ) -> HRESULT {
                    BOOL Ok = TRUE;
                    A->get_IsSuccess( &Ok );
                    LPWSTR U = nullptr;
                    W->get_Source( &U );
                    std::string Url = TakeString( U );
                    if ( Ok ) Owner.BackendNavigated( Url );
                    else
                    {
                        COREWEBVIEW2_WEB_ERROR_STATUS E = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
                        A->get_WebErrorStatus( &E );
                        if ( E != COREWEBVIEW2_WEB_ERROR_STATUS_OPERATION_CANCELED ) Owner.BackendError( "couldn't load " + Url );
                    }
                    return S_OK;
                } );
            Wv->add_NavigationCompleted( OnNav, &T );
            OnNav->Release();
            auto* OnTitle = MakeCallback<ICoreWebView2DocumentTitleChangedEventHandler, ICoreWebView2*, IUnknown*>(
                [ this ]( ICoreWebView2* W, IUnknown* ) -> HRESULT {
                    LPWSTR S2 = nullptr;
                    W->get_DocumentTitle( &S2 );
                    Owner.BackendTitle( TakeString( S2 ) );
                    return S_OK;
                } );
            Wv->add_DocumentTitleChanged( OnTitle, &T );
            OnTitle->Release();
            Apply();
            Owner.BackendReady();
        }

        bool Ready() const override { return Wv != nullptr; }
        void Navigate( const std::string& U ) override { if ( Wv ) Wv->Navigate( Wide( U ).c_str() ); }
        void SetHtml( const std::string& H ) override { if ( Wv ) Wv->NavigateToString( Wide( H ).c_str() ); }
        void Eval( const std::string& J ) override { if ( Wv ) Wv->ExecuteScript( Wide( J ).c_str(), nullptr ); }
        void Back() override { if ( Wv ) Wv->GoBack(); }
        void Forward() override { if ( Wv ) Wv->GoForward(); }
        void Reload() override { if ( Wv ) Wv->Reload(); }

        void Apply()
        {
            if ( !Ctl ) return;
            RECT R = { LONG( LastB.x ), LONG( LastB.y ), LONG( LastB.x + LastB.w ), LONG( LastB.y + LastB.h ) };
            Ctl->put_Bounds( R );
            Ctl->put_IsVisible( Visible );
        }

        void Place( Rect B, Rect C, bool Vis ) override
        {
            // a click in tg (clicks on the page go to WebView2's own window) takes the keyboard back
            if ( Ctx && Ctx->KeyPressCount( Key::Mouse1 ) != Clicks )
            {
                Clicks = Ctx->KeyPressCount( Key::Mouse1 );
                if ( GetFocus() != Parent && IsChild( Parent, GetFocus() ) ) SetFocus( Parent );
            }
            Rect V = B.Intersect( C );
            Vis = Vis && !V.Empty();
            if ( !Vis )
            {
                if ( Visible ) { ShowWindow( Clip, SW_HIDE ); Visible = false; Apply(); }
                return;
            }
            SetWindowPos( Clip, HWND_TOP, int( V.x ), int( V.y ), int( V.w ), int( V.h ), SWP_NOACTIVATE | SWP_SHOWWINDOW );
            Rect Inner = { B.x - V.x, B.y - V.y, B.w, B.h };
            if ( !Visible || Inner.x != LastB.x || Inner.y != LastB.y || Inner.w != LastB.w || Inner.h != LastB.h )
            {
                LastB = Inner;
                Visible = true;
                Apply();
            }
        }
    };

    std::unique_ptr<WebBackend> CreateWebBackend( void* Native, WebView& Owner, std::string& Error )
    {
        CreateEnvFn Create = Loader( Error );
        if ( !Create ) { if ( Error.empty() ) Error = "WebView2Loader is missing CreateCoreWebView2EnvironmentWithOptions"; return nullptr; }
        auto W = std::make_unique<Win32Web>( Owner, (HWND)Native );
        if ( !W->Start( Create, Error ) ) return nullptr;
        return W;
    }
}
