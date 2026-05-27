#pragma once

#include "WebView.h"

#include <Miro/Miro.h>

#include <ea_data_structures/Pointers/Broadcaster.h>
#include <ea_data_structures/Pointers/OwningPointer.h>
#include <ea_data_structures/Structures/Vector.h>

#include <memory>
#include <string>

namespace eacp::Graphics
{

namespace Test
{
class TestServer;
}

using EmptyMessage = Miro::EmptyValue;

// Transport adapter that routes WebView <-> C++ messages through a
// Miro::Bridge. The Bridge owns the CommandTable and the event
// registry; this class is responsible only for the WebView wire
// format (script message handler + injected JS shim + an event
// broadcaster registered on construction and removed on destruction).
//
// On construction the bridge picks up every state hub declared via
// EACP_STATE in the linked TUs (see StateBridge.h) and broadcasts
// their changes on the wire automatically. The auto-bind listeners
// live on `stateListeners` so they unsubscribe when this bridge dies.
//
// The Bridge can be shared with other transports (e.g. an
// HTTP::Rpc::Server) so a single set of typed handlers — including
// those declared via MIRO_EXPORT_COMMAND — is served over multiple
// wires concurrently.
//
// When EACP_WEBVIEW_ENABLE_TEST_SERVER is non-zero, the bridge ctor
// also stands up an embedded HTTP RPC test server (see TestServer.h)
// that injects a `window.__test` DOM-driving agent and registers
// `test.*` commands so external Node-side runners can drive the
// WebView. The bound port is announced on stdout as
// `EACP_RPC_PORT=<n>` so launchers can read it back.
class WebViewBridge
{
public:
    WebViewBridge(WebView& webView);
    ~WebViewBridge();

    Miro::Bridge& getBridge() { return bridge; }

private:
    void onMessage(const std::string& body);
    void deliver(double id,
                 const Miro::Json::Value& result,
                 const std::string* error);
    void broadcast();

    WebView& webView;
    Miro::Bridge bridge;
    EA::Listener emitListener;
    EA::Vector<EA::OwningPointer<EA::Listener>> stateListeners;

    // shared_ptr (not unique_ptr) so this header can use an
    // incomplete TestServer — the control block carries the
    // type-aware deleter installed inside TestServer.cpp.
    std::shared_ptr<Test::TestServer> testServer;
};

} // namespace eacp::Graphics
