#pragma once

#include <eacp/WebView/WebView.h>

#include <Miro/Miro.h>

#include <condition_variable>
#include <functional>
#include <mutex>

namespace eacp::Graphics::Test
{

// Mounts a small set of "test.*" raw commands on the given Miro::Bridge
// that drive the page DOM through an injected `window.__test` agent.
//
// Designed to be wired alongside an eacp::HTTP::Rpc::Server (sharing the
// same bridge) so an external test runner can POST {command:"test.click",
// payload:{selector:"..."}} and have the click happen in the page.
//
// Threading: every command handler blocks until the in-page JS resolves.
// The HTTP::Server that fronts the bridge MUST be configured with
// ServerThreadingMode::ThreadPool — otherwise a handler running on the
// main event loop would deadlock waiting for evaluateJavaScript, whose
// completion callback also needs the main loop.
//
// Lifetime: this object owns nothing the bridge needs after mount();
// destruction is safe whenever the bridge is destroyed. Keep it alive
// at least as long as the bridge to avoid dangling lambda captures.
class TestHarness
{
public:
    explicit TestHarness(WebView& view);

    void mount(Miro::Bridge& bridge);

    int defaultTimeoutMs = 5000;
    int waitForPollMs = 50;

private:
    Miro::JSON runJs(const std::string& script, int timeoutMs);
    void injectAgent();
    void hookNavigation();
    void waitForFirstNavigation(int timeoutMs);

    WebView& webView;
    bool agentInjected = false;
    bool navigationHooked = false;

    std::mutex readyMutex;
    std::condition_variable readyCv;
    bool navigationFinished = false;
    std::function<void(const std::string&)> previousFinishedHandler;
};

} // namespace eacp::Graphics::Test
