#include "Types.h"

#include <eacp/Network/HTTPRpc/RpcServer.h>
#include <eacp/Network/HTTPServer/HttpServer.h>
#include <eacp/WebView/Test/TestHarness.h>
#include <eacp/WebView/WebView.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

using namespace eacp;
using namespace Graphics;

namespace
{

int parsePortInt(const char* str)
{
    char* end = nullptr;
    auto value = std::strtol(str, &end, 10);
    if (end == str)
        return 0;
    return static_cast<int>(value);
}

int parsePortArg(int argc, char** argv)
{
    auto flag = std::string {"--rpc-port="};

    for (auto i = 1; i < argc; ++i)
    {
        auto* arg = argv[i];
        if (std::strncmp(arg, flag.c_str(), flag.size()) == 0)
            return parsePortInt(arg + flag.size());

        if (std::strcmp(arg, "--rpc-port") == 0 && i + 1 < argc)
            return parsePortInt(argv[i + 1]);
    }
    return 0;
}

// The Apps::run<T> entry point default-constructs T, so the port that
// main() parses off argv has to ride in via a process-global. File-
// scope static is the smallest hammer that works.
int requestedPort = 0;

} // namespace

struct TestHostApp
{
    TestHostApp()
    {
        transport.getBridge().use(todos);
        harness.mount(transport.getBridge());

        if (! httpServer.listen(requestedPort))
        {
            std::cerr << "test host: failed to bind RPC server on port "
                      << requestedPort << std::endl;
            std::exit(1);
        }

        // The launcher reads this line off our stdout to discover the
        // ephemeral port allocated by listen(0). Keep the format stable
        // ("EACP_RPC_PORT=<n>\n") — that's the contract.
        std::cout << "EACP_RPC_PORT=" << httpServer.boundPort() << std::endl;
        std::cout.flush();

        setApplicationMenuBar(buildDefaultWebViewMenuBar());
        window.setContentView(webView);
    }

    Api::TodosApi todos;
    WebView webView {embeddedOptions("TodoApp")};
    WebViewBridge transport {webView};
    Graphics::Test::TestHarness harness {webView};

    eacp::HTTP::Server httpServer {
        eacp::HTTP::ServerOptions {eacp::HTTP::ServerThreadingMode::ThreadPool, 4}};
    eacp::HTTP::Rpc::Server rpc {httpServer, transport.getBridge()};

    Window window;
};

int main(int argc, char** argv)
{
    requestedPort = parsePortArg(argc, argv);

    eacp::Apps::run<TestHostApp>();
    return 0;
}
