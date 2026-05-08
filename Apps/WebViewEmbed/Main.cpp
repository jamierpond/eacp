#include <eacp/Network/HTTPRpc/RpcServer.h>
#include <eacp/Network/HTTPServer/HttpServer.h>
#include <eacp/WebView/WebView.h>

#include <iostream>

using namespace eacp;
using namespace Graphics;

namespace
{
constexpr auto kRpcPort = 8088;
}

struct MyApp
{
    MyApp()
    {
        setApplicationMenuBar(buildDefaultWebViewMenuBar());
        window.setContentView(webView);

        bridge.useStaticRegistry();
        rpc.useStaticRegistry();

        if (httpServer.listen(kRpcPort))
            std::cout << "RPC listening on http://localhost:" << kRpcPort
                      << "/rpc\n";
        else
            std::cerr << "Failed to bind RPC server on port " << kRpcPort << "\n";
    }

    WebView webView {embeddedOptions("WebApp")};
    WebViewBridge bridge {webView};
    Window window;
    HTTP::Server httpServer;
    HTTP::Rpc::Server rpc {httpServer};
};

int main()
{
    eacp::Apps::run<MyApp>();

    return 0;
}
