// tg + a web view: a dashboard where part of the UI is a web page (Grafana, docs, an admin panel...),
// talking to C++ both ways through window.tg.
//
//   tg_webview_demo                 the demo
//   tg_webview_demo --selftest      load the page, round-trip a message, exit 0 if it worked (CI)
#include "tg/app.h"
#include "tg/webview.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

// The page: plain HTML/JS. tg.post() sends to C++, tg.on() receives from it.
static const char* Page = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><style>
  :root { color-scheme: dark; }
  body { font: 15px system-ui, sans-serif; background: #12151b; color: #d7dae0; margin: 0; padding: 24px; }
  h1 { font-size: 20px; margin: 0 0 4px; } p { color: #8b93a1; margin: 0 0 20px; }
  .card { background: #1a1e26; border: 1px solid #262c37; border-radius: 10px; padding: 16px; margin-bottom: 14px; }
  .big { font-size: 34px; font-weight: 600; color: #6eb7d4; }
  .bar { height: 8px; background: #262c37; border-radius: 4px; overflow: hidden; margin-top: 10px; }
  .bar div { height: 100%; background: #6eb7d4; transition: width .3s; }
  button { font: inherit; background: #6eb7d4; color: #0b0d11; border: 0; border-radius: 6px; padding: 8px 14px; cursor: pointer; }
  input { font: inherit; background: #0f1217; color: inherit; border: 1px solid #2d3440; border-radius: 6px; padding: 7px 10px; width: 60%; }
  #log { font: 13px ui-monospace, monospace; color: #8b93a1; white-space: pre-wrap; }
</style></head><body>
  <h1>This half is a web page</h1>
  <p>Rendered by the system browser engine. The numbers come from C++; the buttons talk back.</p>
  <div class="card"><div>CPU (pushed from C++ every second)</div><div class="big" id="cpu">--</div><div class="bar"><div id="cpubar" style="width:0"></div></div></div>
  <div class="card">
    <input id="msg" value="hello from JavaScript"> <button onclick="tg.post(document.getElementById('msg').value)">Send to C++</button>
  </div>
  <div class="card"><div id="log"></div></div>
<script>
  const log = s => { const l = document.getElementById('log'); l.textContent = s + '\n' + l.textContent; };
  tg.on(m => {
    if (m.startsWith('cpu:')) { const v = +m.slice(4); cpu.textContent = v.toFixed(0) + '%'; cpubar.style.width = v + '%'; }
    else if (m.startsWith('ping:')) tg.post('pong:' + m.slice(5));
    else log('C++ says: ' + m);
  });
  tg.post('ready');
</script></body></html>)HTML";

int main( int Argc, char** Argv )
{
    bool SelfTest = Argc > 1 && !std::strcmp( Argv[ 1 ], "--selftest" );
    tg::App App( "tg + web view", 1180, 760 );
    auto& M = App.Main();
    M.Layout.Padding = tg::Insets( 12 );
    M.Layout.Gap = 10;

    // browser bar
    auto& Bar = M.Row( 8 );
    Bar.Layout.CrossAlign = tg::Align::Center;
    tg::WebView* Web = nullptr;
    Bar.Button( "\xE2\x86\x90", [ & ] { Web->Back(); } ).Tooltip = "Back";
    Bar.Button( "\xE2\x86\x92", [ & ] { Web->Forward(); } ).Tooltip = "Forward";
    Bar.Button( "Reload", [ & ] { Web->Reload(); } );
    auto& Url = Bar.Input( "URL, Enter to go" );
    Url.Layout.Width = tg::Grow();
    Bar.Button( "Demo page", [ & ] { Web->SetHtml( Page ); } );

    auto& Split = M.Split( tg::Dir::Row, 0.28f );
    Split.Layout.Height = tg::Grow();

    // tg side
    auto& Side = Split.First();
    Side.Layout.Gap = 8;
    Side.Heading( "C++ side" );
    Side.Caption( "Messages from the page (select to copy):" );
    auto& Log = Side.TextArea();
    Log.ReadOnly = true;
    Log.Layout.Height = tg::Grow();
    auto& Say = Side.Input( "Send to the page, Enter" );
    Say.ClearOnSubmit = true;
    auto& Status = Side.Text( "" );
    Status.Style.TextColor = tg::Tok::TextDim;
    Status.Wrap = true;

    // the web view
    Web = &Split.Second().Add<tg::WebView>();
    Web->DevTools = true;
    Web->SetHtml( Page );

    std::string Got;
    bool Pong = false;
    Web->OnMessage = [ & ]( const std::string& S ) {
        Got = S;
        if ( S == "pong:42" ) Pong = true;
        if ( S == "ready" && SelfTest ) Web->Post( "ping:42" );
        Log.SetText( Log.GetText() + S + "\n" );
    };
    Web->OnNavigate = [ & ]( const std::string& U ) { Url.SetText( U.rfind( "data:", 0 ) == 0 || U == "about:blank" ? "" : U ); };
    Web->OnTitle = [ & ]( const std::string& T ) { Status.SetText( "Title: " + T ); };
    Web->OnError = [ & ]( const std::string& E ) { Status.SetText( E ); };
    Url.OnSubmit = [ & ]( const std::string& U ) {
        Web->Navigate( U.find( "://" ) == std::string::npos ? "https://" + U : U );
    };
    Say.OnSubmit = [ & ]( const std::string& S ) { Web->Post( S ); };

    auto T0 = std::chrono::steady_clock::now();
    double LastPush = 0;
    App.OnUpdate = [ & ] {
        double T = std::chrono::duration<double>( std::chrono::steady_clock::now() - T0 ).count();
        if ( T - LastPush >= 1 )
        {
            LastPush = T;
            Web->Post( "cpu:" + std::to_string( int( 45 + 30 * std::sin( T * 0.7 ) ) ) );
        }
        if ( SelfTest && ( Pong || T > 20 ) )
        {
            if ( Pong && T < 3 ) return; // let the page paint for the screenshot
            std::printf( "selftest: %s (last message: %s)\n", Pong ? "ok" : "FAILED", Got.c_str() );
            App.Quit();
        }
    };
    int R = App.Run();
    return SelfTest ? ( Pong ? 0 : 1 ) : R;
}
