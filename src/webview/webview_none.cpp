// No web engine on this platform/build: WebView shows a message instead of a page.
#include "tg/webview.h"

namespace tg
{
    bool WebBackendAvailable() { return false; }
    std::unique_ptr<WebBackend> CreateWebBackend( void*, WebView&, std::string& Error )
    {
        Error = "no web view backend for this platform";
        return nullptr;
    }
}
