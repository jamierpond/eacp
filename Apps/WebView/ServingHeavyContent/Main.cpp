#include <eacp/WebView/WebView.h>

#include <WebResources.h>

#include <iostream>

using namespace eacp;
using namespace Graphics;

// A minimal embedded webview app that serves heavy content — a ~10 MB 1080p
// .mp4 — out of the embedded resource bundle and lets the page range-stream it.
//
// It exists to demonstrate (and regression-guard) that the embedded custom
// scheme answers HTTP byte-range requests: WebKit's / WebView2's media loaders
// fetch <video>/<audio> via ranges, and without real 206 responses they thrash
// and fail. See README.md and the page's on-screen checks. Same target builds
// and verifies on both the macOS (WKWebView) and Windows (WebView2) backends.
struct MyApp
{
    MyApp()
    {
        setApplicationMenuBar(buildDefaultWebViewMenuBar(), window);
        window.setContentView(webView);

        // The page posts every check result here as it runs, so the demo
        // reports itself to the terminal as well as to the window — which is
        // what makes it usable as a regression check (and readable over ssh or
        // anywhere the window isn't visible).
        webView.addScriptMessageHandler("report",
                                        [](const std::string& message)
                                        { std::cout << message << std::endl; });
    }

    WebView webView {embeddedOptions("ServingHeavyContent")};
    Window window;
};

int main()
{
    return eacp::Apps::run<MyApp>();
}
