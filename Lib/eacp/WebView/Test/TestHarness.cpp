#include "TestHarness.h"

#include <eacp/Core/Threads/EventLoop.h>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace eacp::Graphics::Test
{

namespace
{

// Injected once at document-start. Provides synchronous DOM helpers
// only — all "async-feeling" operations (waitFor) are driven from C++
// by repeatedly evaluating __test.exists, so the agent itself never
// returns a Promise. WebKit's evaluateJavaScript callback hands back
// the *synchronous* result of the script's last expression, so async
// helpers wouldn't round-trip.
constexpr const char* agentSource = R"JS(
(function() {
  if (window.__test) return;

  function $(sel) {
    var el = document.querySelector(sel);
    if (!el) throw new Error("__test: element not found: " + sel);
    return el;
  }

  function fireMouse(el, type) {
    var rect = el.getBoundingClientRect();
    el.dispatchEvent(new MouseEvent(type, {
      bubbles: true, cancelable: true, view: window,
      clientX: rect.left + rect.width / 2,
      clientY: rect.top + rect.height / 2,
    }));
  }

  function setNativeValue(el, value) {
    // React tracks input values via an internal cached getter/setter;
    // assigning .value directly bypasses React's onChange. The trick is
    // to call the prototype setter, then dispatch the native input
    // event — that's what React's synthetic event system actually
    // listens for.
    var proto = Object.getPrototypeOf(el);
    var desc = Object.getOwnPropertyDescriptor(proto, 'value');
    if (desc && desc.set) desc.set.call(el, value);
    else el.value = value;
  }

  window.__test = {
    click: function(sel) {
      var el = $(sel);
      fireMouse(el, 'mousedown');
      fireMouse(el, 'mouseup');
      el.click();
      return true;
    },
    fill: function(sel, value) {
      var el = $(sel);
      el.focus();
      setNativeValue(el, String(value));
      el.dispatchEvent(new Event('input', { bubbles: true }));
      el.dispatchEvent(new Event('change', { bubbles: true }));
      return true;
    },
    press: function(sel, key) {
      var el = $(sel);
      el.focus();
      var opts = { bubbles: true, cancelable: true, key: key };
      el.dispatchEvent(new KeyboardEvent('keydown', opts));
      el.dispatchEvent(new KeyboardEvent('keyup', opts));
      return true;
    },
    submit: function(sel) {
      var el = $(sel);
      if (typeof el.requestSubmit === 'function') el.requestSubmit();
      else el.submit();
      return true;
    },
    text: function(sel) {
      return ($(sel).textContent || '').trim();
    },
    attr: function(sel, name) {
      return $(sel).getAttribute(name);
    },
    exists: function(sel) {
      return !!document.querySelector(sel);
    },
    count: function(sel) {
      return document.querySelectorAll(sel).length;
    },
    evaluate: function(expr) {
      return Function('"use strict"; return (' + expr + ');')();
    }
  };
})();
)JS";

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

// Reads a JSON object payload for {selector, ...} arguments. Throws if
// the payload isn't shaped right — the HTTP::Rpc::Server turns that
// into a 500 with the message in the body.
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

struct AsyncJsState
{
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
    std::string result;
    std::string error;
};

} // namespace

TestHarness::TestHarness(WebView& view)
    : webView(view)
{
}

void TestHarness::injectAgent()
{
    if (agentInjected)
        return;

    webView.addUserScript(agentSource, true);
    agentInjected = true;
}

void TestHarness::hookNavigation()
{
    if (navigationHooked)
        return;

    // evaluateJavaScript before the first navigation finishes
    // sometimes fails with WebKit's generic "A JavaScript exception
    // occurred" — the JS context isn't fully set up yet. Latch on the
    // first didFinishNavigation callback so test commands can block
    // until the page is genuinely ready.
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

    navigationHooked = true;
}

void TestHarness::waitForFirstNavigation(int timeoutMs)
{
    auto lock = std::unique_lock {readyMutex};

    auto ready = readyCv.wait_for(lock,
                                  std::chrono::milliseconds {timeoutMs},
                                  [&] { return navigationFinished; });

    if (! ready)
        throw std::runtime_error("test harness: page did not finish loading "
                                 "within " + std::to_string(timeoutMs) + "ms");
}

Miro::JSON TestHarness::runJs(const std::string& script, int timeoutMs)
{
    waitForFirstNavigation(timeoutMs);

    // shared_ptr so the callback can still safely write to state if
    // the caller has already given up and returned (timeout case).
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

    // Convention: every script we generate wraps its result as
    // JSON.stringify({ value: ... }). So the callback hands us a
    // JSON-encoded object — parse it and pull out value.
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

namespace
{

// Wraps an arbitrary JS expression so the callback always gets a
// JSON-encoded {value: ...}. JSON.stringify of undefined returns
// undefined (not a string), which evaluateJavaScript would surface as
// nil — the explicit object wrapper avoids that ambiguity.
std::string wrapExpr(const std::string& expr)
{
    return "JSON.stringify({value:(" + expr + ")})";
}

} // namespace

void TestHarness::mount(Miro::Bridge& bridge)
{
    injectAgent();
    hookNavigation();

    auto& table = bridge.commandTable();

    table.on("test.click",
             [this](const Miro::JSON& payload) -> Miro::JSON
             {
                 auto selector = requiredString(payload, "selector");
                 auto timeout = optionalInt(payload, "timeoutMs", defaultTimeoutMs);
                 return runJs(wrapExpr("window.__test.click("
                                       + jsStringLiteral(selector) + ")"),
                              timeout);
             });

    table.on("test.fill",
             [this](const Miro::JSON& payload) -> Miro::JSON
             {
                 auto selector = requiredString(payload, "selector");
                 auto value = requiredString(payload, "value");
                 auto timeout = optionalInt(payload, "timeoutMs", defaultTimeoutMs);
                 return runJs(wrapExpr("window.__test.fill("
                                       + jsStringLiteral(selector) + ","
                                       + jsStringLiteral(value) + ")"),
                              timeout);
             });

    table.on("test.press",
             [this](const Miro::JSON& payload) -> Miro::JSON
             {
                 auto selector = requiredString(payload, "selector");
                 auto key = requiredString(payload, "key");
                 auto timeout = optionalInt(payload, "timeoutMs", defaultTimeoutMs);
                 return runJs(wrapExpr("window.__test.press("
                                       + jsStringLiteral(selector) + ","
                                       + jsStringLiteral(key) + ")"),
                              timeout);
             });

    table.on("test.submit",
             [this](const Miro::JSON& payload) -> Miro::JSON
             {
                 auto selector = requiredString(payload, "selector");
                 auto timeout = optionalInt(payload, "timeoutMs", defaultTimeoutMs);
                 return runJs(wrapExpr("window.__test.submit("
                                       + jsStringLiteral(selector) + ")"),
                              timeout);
             });

    table.on("test.text",
             [this](const Miro::JSON& payload) -> Miro::JSON
             {
                 auto selector = requiredString(payload, "selector");
                 auto timeout = optionalInt(payload, "timeoutMs", defaultTimeoutMs);
                 return runJs(wrapExpr("window.__test.text("
                                       + jsStringLiteral(selector) + ")"),
                              timeout);
             });

    table.on("test.attr",
             [this](const Miro::JSON& payload) -> Miro::JSON
             {
                 auto selector = requiredString(payload, "selector");
                 auto name = requiredString(payload, "name");
                 auto timeout = optionalInt(payload, "timeoutMs", defaultTimeoutMs);
                 return runJs(wrapExpr("window.__test.attr("
                                       + jsStringLiteral(selector) + ","
                                       + jsStringLiteral(name) + ")"),
                              timeout);
             });

    table.on("test.exists",
             [this](const Miro::JSON& payload) -> Miro::JSON
             {
                 auto selector = requiredString(payload, "selector");
                 auto timeout = optionalInt(payload, "timeoutMs", defaultTimeoutMs);
                 return runJs(wrapExpr("window.__test.exists("
                                       + jsStringLiteral(selector) + ")"),
                              timeout);
             });

    table.on("test.count",
             [this](const Miro::JSON& payload) -> Miro::JSON
             {
                 auto selector = requiredString(payload, "selector");
                 auto timeout = optionalInt(payload, "timeoutMs", defaultTimeoutMs);
                 return runJs(wrapExpr("window.__test.count("
                                       + jsStringLiteral(selector) + ")"),
                              timeout);
             });

    table.on("test.evaluate",
             [this](const Miro::JSON& payload) -> Miro::JSON
             {
                 auto expr = requiredString(payload, "expression");
                 auto timeout = optionalInt(payload, "timeoutMs", defaultTimeoutMs);
                 // The user-supplied expression already encodes its own
                 // value; we still wrap it so JSON.stringify(undefined)
                 // doesn't slip through as nil.
                 return runJs(wrapExpr("window.__test.evaluate("
                                       + jsStringLiteral(expr) + ")"),
                              timeout);
             });

    table.on("test.waitFor",
             [this](const Miro::JSON& payload) -> Miro::JSON
             {
                 auto selector = requiredString(payload, "selector");
                 auto timeout = optionalInt(payload, "timeoutMs", defaultTimeoutMs);

                 auto deadline = std::chrono::steady_clock::now()
                                 + std::chrono::milliseconds {timeout};
                 auto pollScript = wrapExpr("window.__test.exists("
                                            + jsStringLiteral(selector) + ")");

                 while (true)
                 {
                     auto result = runJs(pollScript, defaultTimeoutMs);
                     if (result.isBool() && result.asBool())
                         return Miro::JSON {true};

                     if (std::chrono::steady_clock::now() >= deadline)
                         throw std::runtime_error("waitFor timed out for selector: "
                                                  + selector);

                     std::this_thread::sleep_for(
                         std::chrono::milliseconds {waitForPollMs});
                 }
             });
}

} // namespace eacp::Graphics::Test
