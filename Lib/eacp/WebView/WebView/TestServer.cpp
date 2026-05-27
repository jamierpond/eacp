#include "TestServer.h"

#include "WebView.h"

#include <eacp/Core/Threads/EventLoop.h>
#include <eacp/Network/HTTPRpc/RpcServer.h>
#include <eacp/Network/HTTPServer/HttpServer.h>

#include <Miro/Miro.h>
#include <ResEmbed/ResEmbed.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace eacp::Graphics::Test
{

namespace
{

// Pulled in at runtime from the ResEmbed registry. The actual JS
// lives in WebView/test-agent.js and is embedded into the lib by
// res_embed_add() in this directory's CMakeLists.txt.
std::string loadTestAgentSource()
{
    auto view = ResEmbed::get("test-agent.js", "EacpTestServer");
    if (! view)
        throw std::runtime_error(
            "test server: embedded test-agent.js resource not found");
    return view.toString();
}

std::string jsStringLiteral(std::string_view value)
{
    auto out = std::string {"\""};
    out.reserve(value.size() + 2);

    for (auto c: value)
    {
        switch (c)
        {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '<': out += "\\u003c"; break;
            case '>': out += "\\u003e"; break;
            case '&': out += "\\u0026"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                }
                else
                {
                    out += c;
                }
                break;
        }
    }

    out += '"';
    return out;
}

const Miro::JSON& field(const Miro::JSON& payload, const std::string& key)
{
    if (! payload.isObject())
        throw std::runtime_error("payload must be a JSON object");

    auto& obj = payload.asObject();
    auto it = obj.find(key);

    if (it == obj.end())
        throw std::runtime_error("missing required field: " + key);

    return it->second;
}

std::string requiredString(const Miro::JSON& payload, const std::string& key)
{
    auto& value = field(payload, key);

    if (! value.isString())
        throw std::runtime_error("field '" + key + "' must be a string");

    return value.asString();
}

int optionalInt(const Miro::JSON& payload, const std::string& key, int fallback)
{
    if (! payload.isObject())
        return fallback;

    auto& obj = payload.asObject();
    auto it = obj.find(key);

    if (it == obj.end() || ! it->second.isNumber())
        return fallback;

    return static_cast<int>(it->second.asNumber());
}

// Wraps an arbitrary JS expression so the callback always gets a
// JSON-encoded {value: ...}. JSON.stringify of undefined returns
// undefined (not a string), which evaluateJavaScript would surface
// as nil — the explicit object wrapper avoids that ambiguity.
std::string wrapExpr(const std::string& expr)
{
    return "JSON.stringify({value:(" + expr + ")})";
}

struct AsyncJsState
{
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
    std::string result;
    std::string error;
};

// Well-known default — agreed with the Node-side launcher's default
// attach URL (tools/eacp-test-node/src/launch.ts). Picked once so a
// manually launched app and `npm test` connect with zero env vars.
// Test runners that spawn the app in parallel pass EACP_RPC_PORT=0
// explicitly to fall back to an ephemeral port and dodge collisions.
constexpr long defaultRpcPort = 8765;

long readPortEnv()
{
    auto* env = std::getenv("EACP_RPC_PORT");
    if (! env)
        return defaultRpcPort;

    char* end = nullptr;
    auto value = std::strtol(env, &end, 10);
    return (end == env) ? defaultRpcPort : value;
}

// Single point that consults the build-time macro. Everything else
// in this TU works in terms of this function so the macro's only
// reader stays in one place.
bool shouldEnableTestServer()
{
    return EACP_WEBVIEW_ENABLE_TEST_SERVER;
}

} // namespace

// Owns the HTTP RPC server + test command wiring. ThreadPool mode is
// mandatory: every test command handler blocks on a CV waiting for
// evaluateJavaScript to resolve, and that callback needs the main
// event loop — running handlers on the main loop would deadlock.
class TestServer
{
public:
    static constexpr int defaultTimeoutMs = 5000;
    static constexpr int waitForPollMs = 50;

    TestServer(WebView& webViewToUse, Miro::Bridge& bridgeToUse)
        : webView(webViewToUse)
        , rpcServer(httpServer, bridgeToUse)
    {
        webView.addUserScript(loadTestAgentSource(), true);
        hookNavigation();
        mountCommands(bridgeToUse);

        // Defer the listen+announce until the runloop is actually
        // servicing events — the surrounding WebViewBridge runs from
        // its owner app's member-init, so the window/setContentView
        // calls in the app's constructor body haven't happened yet.
        // Binding the port too early lets the launcher race in with a
        // probe before the WebView is in a visible window, which
        // stalls didFinishNavigation under some launch conditions
        // (notably Playwright's non-tty spawn).
        //
        // EACP_RPC_PORT env var pins the listen port — useful when
        // launching the app manually for an external test runner to
        // attach to. Unset (or 0) → ephemeral port chosen by the OS.
        auto requestedPort = readPortEnv();
        eacp::Threads::callAsync(
            [this, requestedPort]
            {
                if (! httpServer.listen(static_cast<int>(requestedPort)))
                    throw std::runtime_error("test server: HTTP listen failed");

                std::cout << "EACP_RPC_PORT=" << httpServer.boundPort()
                          << std::endl;
                std::cout.flush();
            });
    }

private:
    void hookNavigation()
    {
        // evaluateJavaScript before the first navigation finishes
        // sometimes fails with WebKit's generic "A JavaScript
        // exception occurred" — the JS context isn't fully set up
        // yet. Latch on the first didFinishNavigation callback so
        // test commands can block until the page is ready.
        previousFinishedHandler = webView.onNavigationFinished;
        webView.onNavigationFinished =
            [this, previous = previousFinishedHandler](const std::string& url)
            {
                if (previous)
                    previous(url);

                auto lock = std::lock_guard {readyMutex};
                navigationFinished = true;
                readyCv.notify_all();
            };
    }

    void waitForFirstNavigation(int timeoutMs)
    {
        auto lock = std::unique_lock {readyMutex};
        auto ready = readyCv.wait_for(lock,
                                      std::chrono::milliseconds {timeoutMs},
                                      [&] { return navigationFinished; });
        if (! ready)
            throw std::runtime_error("test server: page did not finish "
                                     "loading within "
                                     + std::to_string(timeoutMs) + "ms");
    }

    Miro::JSON runJs(const std::string& script, int timeoutMs)
    {
        waitForFirstNavigation(timeoutMs);

        // shared_ptr so the callback can still safely write to state
        // if the caller has already given up and returned (timeout
        // case).
        auto state = std::make_shared<AsyncJsState>();

        eacp::Threads::callAsync(
            [this, state, script]
            {
                webView.evaluateJavaScript(
                    script,
                    [state](const std::string& result, const std::string& error)
                    {
                        auto lock = std::lock_guard {state->mutex};
                        state->result = result;
                        state->error = error;
                        state->done = true;
                        state->cv.notify_one();
                    });
            });

        auto lock = std::unique_lock {state->mutex};
        auto ready = state->cv.wait_for(lock,
                                        std::chrono::milliseconds {timeoutMs},
                                        [&] { return state->done; });

        if (! ready)
            throw std::runtime_error("test command timed out after "
                                     + std::to_string(timeoutMs) + "ms");
        if (! state->error.empty())
            throw std::runtime_error(state->error);

        if (state->result.empty())
            return Miro::JSON {};

        auto parsed = Miro::JSON {};
        try
        {
            parsed = Miro::Json::parse(state->result);
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error(std::string {"failed to parse JS result: "}
                                     + e.what() + " (raw: " + state->result + ")");
        }

        if (! parsed.isObject())
            return parsed;

        auto& obj = parsed.asObject();
        auto it = obj.find("value");
        return it != obj.end() ? it->second : Miro::JSON {};
    }

    void mountCommands(Miro::Bridge& bridge)
    {
        auto& table = bridge.commandTable();

        auto sel = [](const Miro::JSON& p)
        { return jsStringLiteral(requiredString(p, "selector")); };
        auto timeout = [](const Miro::JSON& p)
        { return optionalInt(p, "timeoutMs", defaultTimeoutMs); };

        table.on("test.click",
                 [this, sel, timeout](const Miro::JSON& p) -> Miro::JSON
                 { return runJs(wrapExpr("window.__test.click(" + sel(p) + ")"),
                                timeout(p)); });

        table.on("test.fill",
                 [this, sel, timeout](const Miro::JSON& p) -> Miro::JSON
                 {
                     auto value = jsStringLiteral(requiredString(p, "value"));
                     return runJs(wrapExpr("window.__test.fill("
                                           + sel(p) + "," + value + ")"),
                                  timeout(p));
                 });

        table.on("test.press",
                 [this, sel, timeout](const Miro::JSON& p) -> Miro::JSON
                 {
                     auto key = jsStringLiteral(requiredString(p, "key"));
                     return runJs(wrapExpr("window.__test.press("
                                           + sel(p) + "," + key + ")"),
                                  timeout(p));
                 });

        table.on("test.submit",
                 [this, sel, timeout](const Miro::JSON& p) -> Miro::JSON
                 { return runJs(wrapExpr("window.__test.submit(" + sel(p) + ")"),
                                timeout(p)); });

        table.on("test.text",
                 [this, sel, timeout](const Miro::JSON& p) -> Miro::JSON
                 { return runJs(wrapExpr("window.__test.text(" + sel(p) + ")"),
                                timeout(p)); });

        table.on("test.attr",
                 [this, sel, timeout](const Miro::JSON& p) -> Miro::JSON
                 {
                     auto name = jsStringLiteral(requiredString(p, "name"));
                     return runJs(wrapExpr("window.__test.attr("
                                           + sel(p) + "," + name + ")"),
                                  timeout(p));
                 });

        table.on("test.exists",
                 [this, sel, timeout](const Miro::JSON& p) -> Miro::JSON
                 { return runJs(wrapExpr("window.__test.exists(" + sel(p) + ")"),
                                timeout(p)); });

        table.on("test.count",
                 [this, sel, timeout](const Miro::JSON& p) -> Miro::JSON
                 { return runJs(wrapExpr("window.__test.count(" + sel(p) + ")"),
                                timeout(p)); });

        table.on("test.evaluate",
                 [this, timeout](const Miro::JSON& p) -> Miro::JSON
                 {
                     auto expr = jsStringLiteral(requiredString(p, "expression"));
                     return runJs(wrapExpr("window.__test.evaluate("
                                           + expr + ")"),
                                  timeout(p));
                 });

        table.on("test.waitFor",
                 [this, timeout](const Miro::JSON& p) -> Miro::JSON
                 {
                     auto selector = requiredString(p, "selector");
                     auto deadline = std::chrono::steady_clock::now()
                                     + std::chrono::milliseconds {timeout(p)};
                     auto pollScript = wrapExpr("window.__test.exists("
                                                + jsStringLiteral(selector) + ")");

                     while (true)
                     {
                         auto result = runJs(pollScript, defaultTimeoutMs);
                         if (result.isBool() && result.asBool())
                             return Miro::JSON {true};
                         if (std::chrono::steady_clock::now() >= deadline)
                             throw std::runtime_error("waitFor timed out for "
                                                      "selector: " + selector);
                         std::this_thread::sleep_for(
                             std::chrono::milliseconds {waitForPollMs});
                     }
                 });
    }

    WebView& webView;
    eacp::HTTP::Server httpServer {
        eacp::HTTP::ServerOptions {eacp::HTTP::ServerThreadingMode::ThreadPool, 4}};
    eacp::HTTP::Rpc::Server rpcServer;

    std::mutex readyMutex;
    std::condition_variable readyCv;
    bool navigationFinished = false;
    std::function<void(const std::string&)> previousFinishedHandler;
};

std::shared_ptr<TestServer> installIfEnabled(WebView& webView,
                                             Miro::Bridge& bridge)
{
    if (! shouldEnableTestServer())
        return nullptr;

    return std::make_shared<TestServer>(webView, bridge);
}

} // namespace eacp::Graphics::Test
