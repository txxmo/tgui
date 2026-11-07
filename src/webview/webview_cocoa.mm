// WebView on macOS: WKWebView in a clipping NSView added to the window's content view.
// Compiled with ARC (CMake sets -fobjc-arc).
#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#include "tg/context.h"
#include "tg/webview.h"

@interface TgWebDelegate : NSObject <WKScriptMessageHandler, WKNavigationDelegate>
@property( nonatomic, assign ) tg::WebView* Owner;
@end

@implementation TgWebDelegate
- (void)userContentController:(WKUserContentController*)C didReceiveScriptMessage:(WKScriptMessage*)M
{
    if ( !self.Owner ) return;
    NSString* S = [M.body isKindOfClass:[NSString class]] ? (NSString*)M.body : [M.body description];
    self.Owner->BackendMessage( S.UTF8String ? S.UTF8String : "" );
}
- (void)webView:(WKWebView*)W didFinishNavigation:(WKNavigation*)N
{
    if ( !self.Owner ) return;
    self.Owner->BackendNavigated( W.URL.absoluteString.UTF8String ? W.URL.absoluteString.UTF8String : "" );
    self.Owner->BackendTitle( W.title.UTF8String ? W.title.UTF8String : "" );
}
- (void)webView:(WKWebView*)W didFailProvisionalNavigation:(WKNavigation*)N withError:(NSError*)E
{
    if ( self.Owner && E.code != NSURLErrorCancelled )
        self.Owner->BackendError( std::string( "couldn't load: " ) + ( E.localizedDescription.UTF8String ? E.localizedDescription.UTF8String : "" ) );
}
@end

namespace tg
{
    bool WebBackendAvailable() { return true; }

    static NSString* Ns( const std::string& S ) { return [NSString stringWithUTF8String:S.c_str()] ?: @""; }

    struct CocoaWeb : WebBackend
    {
        WebView& Owner;
        Context* Ctx;
        NSWindow* Win;
        NSView* Clip;
        WKWebView* View;
        TgWebDelegate* Delegate;
        uint32_t Clicks = 0;

        CocoaWeb( WebView& O, NSWindow* W ) : Owner( O ), Ctx( O.GetContext() ), Win( W )
        {
            Delegate = [TgWebDelegate new];
            Delegate.Owner = &Owner;
            WKWebViewConfiguration* Cfg = [WKWebViewConfiguration new];
            [Cfg.userContentController addScriptMessageHandler:Delegate name:@"tg"];
            WKUserScript* Js = [[WKUserScript alloc] initWithSource:Ns( WebView::BridgeScript( "window.webkit.messageHandlers.tg.postMessage" ) )
                                                      injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                                                   forMainFrameOnly:NO];
            [Cfg.userContentController addUserScript:Js];
            if ( Owner.DevTools ) [Cfg.preferences setValue:@YES forKey:@"developerExtrasEnabled"];
            Clip = [[NSView alloc] initWithFrame:NSMakeRect( 0, 0, 1, 1 )];
            Clip.wantsLayer = YES;
            Clip.layer.masksToBounds = YES;
            Clip.hidden = YES;
            View = [[WKWebView alloc] initWithFrame:NSMakeRect( 0, 0, 1, 1 ) configuration:Cfg];
            View.navigationDelegate = Delegate;
            [Clip addSubview:View];
            [Win.contentView addSubview:Clip];
            if ( Ctx ) Clicks = Ctx->KeyPressCount( Key::Mouse1 );
        }

        ~CocoaWeb() override
        {
            Delegate.Owner = nullptr;
            [View.configuration.userContentController removeScriptMessageHandlerForName:@"tg"];
            [Clip removeFromSuperview];
        }

        bool Ready() const override { return true; }
        void Navigate( const std::string& U ) override
        {
            NSURL* Url = [NSURL URLWithString:Ns( U )];
            if ( Url ) [View loadRequest:[NSURLRequest requestWithURL:Url]];
        }
        void SetHtml( const std::string& H ) override { [View loadHTMLString:Ns( H ) baseURL:nil]; }
        void Eval( const std::string& J ) override { [View evaluateJavaScript:Ns( J ) completionHandler:nil]; }
        void Back() override { [View goBack]; }
        void Forward() override { [View goForward]; }
        void Reload() override { [View reload]; }

        void Place( Rect B, Rect C, bool Vis ) override
        {
            if ( Ctx && Ctx->KeyPressCount( Key::Mouse1 ) != Clicks )
            {
                Clicks = Ctx->KeyPressCount( Key::Mouse1 );
                if ( [Win.firstResponder isKindOfClass:[NSView class]] && [(NSView*)Win.firstResponder isDescendantOf:View] )
                    [Win makeFirstResponder:Win.contentView];
            }
            Rect V = B.Intersect( C );
            if ( !Vis || V.Empty() ) { Clip.hidden = YES; return; }
            // pixels -> points, top-left origin -> Cocoa's bottom-left
            CGFloat S = Win.backingScaleFactor > 0 ? Win.backingScaleFactor : 1, H = Win.contentView.bounds.size.height;
            NSRect Cf = NSMakeRect( V.x / S, H - ( V.y + V.h ) / S, V.w / S, V.h / S );
            NSRect Wf = NSMakeRect( ( B.x - V.x ) / S, Cf.size.height - ( ( B.y - V.y ) + B.h ) / S, B.w / S, B.h / S );
            if ( !NSEqualRects( Clip.frame, Cf ) ) Clip.frame = Cf;
            if ( !NSEqualRects( View.frame, Wf ) ) View.frame = Wf;
            Clip.hidden = NO;
        }
    };

    std::unique_ptr<WebBackend> CreateWebBackend( void* Native, WebView& Owner, std::string& Error )
    {
        NSWindow* W = (__bridge NSWindow*)Native;
        if ( !W ) { Error = "no NSWindow"; return nullptr; }
        return std::make_unique<CocoaWeb>( Owner, W );
    }
}
