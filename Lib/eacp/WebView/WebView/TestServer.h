#pragma once

#include <memory>

namespace Miro
{
class Bridge;
}

namespace eacp::Graphics
{
class WebView;
}

namespace eacp::Graphics::Test
{

// Opaque — the full definition lives in TestServer.cpp. The
// shared_ptr<incomplete> pattern works because the control block
// stores a type-aware deleter installed when the object is created
// inside TestServer.cpp.
class TestServer;

// Installs the embedded HTTP RPC test server on the supplied
// Miro::Bridge if EACP_WEBVIEW_ENABLE_TEST_SERVER is non-zero;
// otherwise returns null. The returned handle keeps the server alive
// — drop it (e.g. by letting the owning WebViewBridge destruct) to
// tear the server and its bindings down.
std::shared_ptr<TestServer> installIfEnabled(WebView& webView,
                                             Miro::Bridge& bridge);

} // namespace eacp::Graphics::Test
