#include "Types.h"

#include <eacp/WebView/WebView.h>

using namespace eacp;
using namespace Graphics;

struct MyApp
{
    MyApp()
    {
        transport.getBridge().use(todos);

        setApplicationMenuBar(buildDefaultWebViewMenuBar());
        window.setContentView(webView);
    }

    // todos declared first → destructed last (after the transport's
    // bridge listeners and handlers have torn down).
    Api::TodosApi todos;
    WebView webView {embeddedOptions("TodoApp")};
    WebViewBridge transport {webView};
    Window window;
};

int main()
{
    eacp::Apps::run<MyApp>();
    return 0;
}
