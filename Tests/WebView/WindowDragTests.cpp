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
