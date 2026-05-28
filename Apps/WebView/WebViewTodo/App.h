#pragma once

#include "Types.h"

#include <eacp/WebView/WebView.h>

struct MyApp
{
    MyApp()
    {
        transport.getBridge().use(todos);

        eacp::Graphics::setApplicationMenuBar(
            eacp::Graphics::buildDefaultWebViewMenuBar());
        window.setContentView(webView);
    }

    // todos declared first → destructed last (after the transport's
    // bridge listeners and handlers have torn down).
    Api::TodosApi todos;
    eacp::Graphics::WebView webView {eacp::Graphics::embeddedOptions("TodoApp")};
    eacp::Graphics::WebViewBridge transport {webView};
    eacp::Graphics::Window window;
};
