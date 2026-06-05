// Exercises the page-side half of window dragging on a real WKWebView. eacp
// auto-injects window-drag.js, which exposes window.__eacpResolveAppRegion --
// the function the mousedown handler uses to decide whether a point starts a
// window drag. We assert it classifies regions the way the native side relies
// on:
//   * a `--eacp-app-region: drag` element resolves to "drag"
//   * a `no-drag` descendant opts back out ("no-drag")
//   * an unmarked descendant of a drag region inherits "drag"
//   * everything else resolves to "" (not draggable)
//
// This also pins down the property that makes the whole approach work on
// WKWebView: that the `--eacp-app-region` CUSTOM property is exposed via
// getComputedStyle (the native `-webkit-app-region` is not). If WebKit ever
// stopped exposing it, the "drag"/inherited cases below would regress to "".

#include <eacp/Core/Threads/EventLoop.h>
#include <eacp/WebView/WebView.h>

#include <NanoTest/NanoTest.h>

#include <string>

using namespace nano;
using namespace eacp;
using namespace eacp::Graphics;
using namespace std::chrono_literals;

namespace
{
// A drag bar (with an opt-out button and an unmarked label inside it) plus an
// unmarked sibling. `ready` fires once the document is parsed so the test only
// queries after window-drag.js's document-start injection has run.
const std::string pageHtml = R"HTML(<!doctype html><html><head><style>
  #bar { --eacp-app-region: drag; }
  #btn { --eacp-app-region: no-drag; }
</style></head><body>
  <div id="bar">
    <button id="btn">x</button>
    <span id="label">title</span>
  </div>
  <div id="outside">body</div>
  <script>window.webkit.messageHandlers.ready.postMessage('ready');</script>
</body></html>)HTML";

struct Fixture
{
    WebView webView {};
    Window window {};
    bool ready = false;

    Fixture()
    {
        window.setContentView(webView);
        webView.addScriptMessageHandler(
            "ready", [this](const std::string&) { ready = true; });
        webView.loadHTML(pageHtml);
        check(Threads::runEventLoopUntil([this] { return ready; }, 10s));
    }

    // Resolves the app-region of the first element matching `selector`, the way
    // the injected mousedown handler does for its event target.
    std::string regionOf(const std::string& selector)
    {
        auto script =
            "window.__eacpResolveAppRegion(document.querySelector('" + selector + "'))";
        return webView.callJS(script).waitFor(10s);
    }
};
} // namespace

auto tDragRegionResolves = test("WindowDrag/dragRegionResolvesToDrag") = []
{
    auto fix = Fixture {};
    check(fix.regionOf("#bar") == "drag");
};

auto tNoDragOptsOut = test("WindowDrag/noDragDescendantOptsOut") = []
{
    auto fix = Fixture {};
    check(fix.regionOf("#btn") == "no-drag");
};

auto tUnmarkedChildInherits = test("WindowDrag/unmarkedChildInheritsDrag") = []
{
    auto fix = Fixture {};
    // The custom property inherits, so a label inside the bar drags too --
    // this is what makes the whole bar a drag handle, not just its background.
    check(fix.regionOf("#label") == "drag");
};

auto tOutsideIsNotDraggable = test("WindowDrag/unmarkedRegionIsEmpty") = []
{
    auto fix = Fixture {};
    check(fix.regionOf("#outside").empty());
};

// Regression guard for the crash this feature shipped with: window-drag.js arms
// the drag by posting a NON-string body to a script message handler. A bare
// number/bool/null is not a valid JSON top level, so didReceiveScriptMessage's
// NSJSONSerialization THREW -- an uncaught NSException that aborted the whole
// app on the first drag. The handler now guards with isValidJSONObject and
// still fires with an empty body. Without the fix this test crashes the process
// (the throw is synchronous, inside the event-loop pump below) instead of
// failing cleanly -- either way it goes red.
struct NumberMessageProbe
{
    WebView webView {};
    Window window {};
    bool called = false;
    std::string body = "unset";

    NumberMessageProbe()
    {
        window.setContentView(webView);
        webView.addScriptMessageHandler("numberProbe",
                                        [this](const std::string& received)
                                        {
                                            called = true;
                                            body = received;
                                        });
        webView.loadHTML(
            "<!doctype html><html><body><script>"
            "window.webkit.messageHandlers.numberProbe.postMessage(1);"
            "</script></body></html>");
        check(Threads::runEventLoopUntil([this] { return called; }, 10s));
    }
};

auto tNumberBodyDoesNotCrash = test("WindowDrag/numberMessageBodyDoesNotCrash") = []
{
    auto probe = NumberMessageProbe {};
    check(probe.called);     // handler fired -- the app did not abort
    check(probe.body.empty()); // invalid JSON top level -> empty body, no throw
};
