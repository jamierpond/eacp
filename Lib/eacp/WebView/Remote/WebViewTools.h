#pragma once

#include <eacp/Graphics/Remote/WindowDebugServer.h>
#include <eacp/WebView/Test/AppDriver.h>

#include <functional>
#include <string>

namespace Miro
{
class Bridge;
}

namespace eacp::Graphics
{
class WebView;
}

namespace eacp::WebView::Remote
{

// The WebView debug capability as a window-server extension: it adds the
// DOM-driving tools — list_elements, click, fill, press, submit, text,
// attr, count, wait_for, dom, evaluate_js, console_logs, invoke_command,
// navigate, snapshot, page_info — plus page console capture, onto the
// WindowDebugServer.
//
// Screenshot and screen recording are deliberately NOT here: those are
// window-level capabilities the WindowDebugServer owns, so they work for
// any app (GPU, native, SVG, WebView). This just enriches that one
// server with DOM driving when a WebView is present, so a WebView app
// exposes a single MCP endpoint carrying capture + DOM.
//
// Apps don't construct this directly: the WebView auto-attach hook
// (AutoAttach.h) creates one per WebViewBridge and hands it to the
// window server. Tests construct it manually and add it to a
// WindowDebugServer.
class WebViewTools : public Graphics::Remote::ServerExtension
{
public:
    WebViewTools(Graphics::WebView& webViewToUse,
                 Miro::Bridge& bridgeToUse,
                 std::string snapshotDir = {});
    ~WebViewTools() override;

    WebViewTools(const WebViewTools&) = delete;
    WebViewTools& operator=(const WebViewTools&) = delete;

    void registerTools(Graphics::Remote::WindowDebugServer& server) override;

private:
    void installConsoleCapture();

    Graphics::WebView& webView;
    Test::AppDriver driver;
    std::function<void(const std::string&)> previousFinishedHandler;
};

} // namespace eacp::WebView::Remote
