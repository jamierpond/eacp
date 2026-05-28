#include "AppDriver.h"

#include <eacp/Core/Threads/EventLoop.h>
#include <eacp/WebView/WebView.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <utility>

namespace eacp::WebView::Test
{

namespace
{

constexpr auto defaultTimeoutMs = 5000;
constexpr auto waitForPollMs = 50;
constexpr auto defaultSnapshotSubdir = "test-results/snapshots";

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

// Wraps an arbitrary JS expression so the callback always gets a
// JSON-encoded {value: ...}. JSON.stringify of undefined returns
// undefined (not a string), which evaluateJavaScript would surface
// as an empty result — the explicit object wrapper avoids that
// ambiguity.
std::string wrapExpr(const std::string& expression)
{
    return "JSON.stringify({value:(" + expression + ")})";
}

std::string resolveSnapshotDir(std::string fromOptions)
{
    if (!fromOptions.empty())
        return fromOptions;

    auto cwd = std::filesystem::current_path();
    return (cwd / defaultSnapshotSubdir).string();
}

std::string sanitizeSnapshotName(const std::string& name)
{
    auto out = std::string {};
    out.reserve(name.size());
    for (auto c: name)
    {
        auto ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                  || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '/'
                  || c == '-';
        out.push_back(ok ? c : '_');
    }
    return out;
}

void writeBinary(const std::filesystem::path& path,
                 const std::vector<std::uint8_t>& bytes)
{
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path());

    auto file = std::ofstream {path, std::ios::binary | std::ios::trunc};
    if (!file)
        throw std::runtime_error("AppDriver: failed to open '" + path.string()
                                 + "' for writing");
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    if (!file)
        throw std::runtime_error("AppDriver: short write to '" + path.string()
                                 + "'");
}

void writeText(const std::filesystem::path& path, const std::string& text)
{
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path());

    auto file = std::ofstream {path, std::ios::trunc};
    if (!file)
        throw std::runtime_error("AppDriver: failed to open '" + path.string()
                                 + "' for writing");
    file << text;
    if (!file)
        throw std::runtime_error("AppDriver: short write to '" + path.string()
                                 + "'");
}

bool asBool(const Miro::JSON& v)
{
    return v.isBool() && v.asBool();
}

std::string asString(const Miro::JSON& v)
{
    if (v.isString())
        return v.asString();
    if (v.isNull())
        return {};
    throw std::runtime_error("AppDriver: expected string result");
}

int asInt(const Miro::JSON& v)
{
    if (v.isNumber())
        return static_cast<int>(v.asNumber());
    throw std::runtime_error("AppDriver: expected numeric result");
}

} // namespace

AppDriver::AppDriver(eacp::Graphics::WebView& webViewToUse,
                     Miro::Bridge& bridgeToUse, AppDriverOptions options)
    : webView(webViewToUse)
    , bridge(bridgeToUse)
    , defaultTimeoutMs(options.defaultTimeoutMs)
    , snapshotDir(resolveSnapshotDir(std::move(options.snapshotDir)))
{
    // evaluateJavaScript before the first navigation finishes
    // sometimes fails — the JS context isn't fully set up yet. Latch
    // on the first didFinishNavigation callback so command methods
    // can pump the loop until the page is ready.
    //
    // Chain through whatever was already installed so users aren't
    // surprised by their own handler getting clobbered.
    previousFinishedHandler = webView.onNavigationFinished;
    webView.onNavigationFinished =
        [this, previous = previousFinishedHandler](const std::string& url)
    {
        if (previous)
            previous(url);

        navigationFinished = true;
        // Wake any pending waitForFirstNavigation pump.
        eacp::Threads::stopEventLoop();
    };
}

AppDriver::~AppDriver()
{
    // Restore the user's handler so a longer-lived WebView doesn't
    // keep firing into this dead driver.
    webView.onNavigationFinished = std::move(previousFinishedHandler);
}

int AppDriver::effectiveTimeoutMs(const CallOptions& opts) const
{
    if (opts.timeoutMs)
        return *opts.timeoutMs;
    if (defaultTimeoutMs)
        return *defaultTimeoutMs;
    return defaultTimeoutMs ? *defaultTimeoutMs : Test::defaultTimeoutMs;
}

void AppDriver::waitForFirstNavigation(const CallOptions& opts)
{
    if (navigationFinished)
        return;

    auto timeout = std::chrono::milliseconds {effectiveTimeoutMs(opts)};
    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (!navigationFinished)
    {
        auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0)
            break;

        // runEventLoopFor returns true if stopEventLoop was called
        // before timeout. Our onNavigationFinished hook above calls
        // stopEventLoop, so a true return means the navigation
        // probably fired.
        eacp::Threads::runEventLoopFor(remaining);
    }

    if (!navigationFinished)
        throw std::runtime_error("AppDriver: page did not finish loading "
                                 "within " + std::to_string(timeout.count())
                                 + "ms");
}

namespace
{

struct JsResult
{
    std::string result;
    std::string error;
    bool done = false;
};

struct SnapshotState
{
    std::vector<std::uint8_t> bytes;
    std::string error;
    bool done = false;
};

} // namespace

Miro::JSON AppDriver::runJs(const std::string& expression,
                            const CallOptions& opts)
{
    waitForFirstNavigation(opts);

    auto timeout = std::chrono::milliseconds {effectiveTimeoutMs(opts)};
    auto state = JsResult {};

    webView.evaluateJavaScript(
        wrapExpr(expression),
        [&state](const std::string& result, const std::string& error)
        {
            state.result = result;
            state.error = error;
            state.done = true;
            eacp::Threads::stopEventLoop();
        });

    if (!state.done)
    {
        if (!eacp::Threads::runEventLoopFor(timeout) && !state.done)
            throw std::runtime_error("AppDriver: JS evaluation timed out "
                                     "after " + std::to_string(timeout.count())
                                     + "ms");
    }

    if (!state.error.empty())
        throw std::runtime_error("AppDriver JS error: " + state.error);

    if (state.result.empty())
        return Miro::JSON {};

    auto parsed = Miro::JSON {};
    try
    {
        parsed = Miro::Json::parse(state.result);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error(std::string {"AppDriver: failed to parse JS "
                                              "result: "} + e.what() + " (raw: "
                                 + state.result + ")");
    }

    if (!parsed.isObject())
        return parsed;

    auto& obj = parsed.asObject();
    auto it = obj.find("value");
    return it != obj.end() ? it->second : Miro::JSON {};
}

std::vector<std::uint8_t>
AppDriver::runSnapshotBytes(const CallOptions& opts)
{
    waitForFirstNavigation(opts);

    auto timeout = std::chrono::milliseconds {effectiveTimeoutMs(opts)};
    auto state = SnapshotState {};

    webView.takeSnapshot(
        [&state](std::vector<std::uint8_t> bytes, const std::string& error)
        {
            state.bytes = std::move(bytes);
            state.error = error;
            state.done = true;
            eacp::Threads::stopEventLoop();
        });

    if (!state.done)
    {
        if (!eacp::Threads::runEventLoopFor(timeout) && !state.done)
            throw std::runtime_error("AppDriver: screenshot timed out after "
                                     + std::to_string(timeout.count()) + "ms");
    }

    if (!state.error.empty())
        throw std::runtime_error("AppDriver: " + state.error);

    return std::move(state.bytes);
}

Miro::JSON AppDriver::invoke(const std::string& command,
                             const Miro::JSON& payload)
{
    return bridge.dispatch(command, payload);
}

bool AppDriver::click(const std::string& selector, CallOptions opts)
{
    return asBool(runJs("window.__test.click(" + jsStringLiteral(selector) + ")",
                        opts));
}

bool AppDriver::fill(const std::string& selector, const std::string& value,
                     CallOptions opts)
{
    return asBool(runJs("window.__test.fill(" + jsStringLiteral(selector) + ","
                        + jsStringLiteral(value) + ")",
                        opts));
}

bool AppDriver::press(const std::string& selector, const std::string& key,
                      CallOptions opts)
{
    return asBool(runJs("window.__test.press(" + jsStringLiteral(selector) + ","
                        + jsStringLiteral(key) + ")",
                        opts));
}

bool AppDriver::submit(const std::string& selector, CallOptions opts)
{
    return asBool(runJs("window.__test.submit(" + jsStringLiteral(selector) + ")",
                        opts));
}

std::string AppDriver::text(const std::string& selector, CallOptions opts)
{
    return asString(runJs("window.__test.text(" + jsStringLiteral(selector)
                          + ")",
                          opts));
}

std::optional<std::string> AppDriver::attr(const std::string& selector,
                                           const std::string& name,
                                           CallOptions opts)
{
    auto result = runJs("window.__test.attr(" + jsStringLiteral(selector) + ","
                        + jsStringLiteral(name) + ")",
                        opts);
    if (result.isNull())
        return std::nullopt;
    return asString(result);
}

bool AppDriver::exists(const std::string& selector, CallOptions opts)
{
    return asBool(runJs("window.__test.exists(" + jsStringLiteral(selector)
                        + ")",
                        opts));
}

int AppDriver::count(const std::string& selector, CallOptions opts)
{
    return asInt(runJs("window.__test.count(" + jsStringLiteral(selector) + ")",
                       opts));
}

bool AppDriver::waitFor(const std::string& selector, CallOptions opts)
{
    auto timeout = std::chrono::milliseconds {effectiveTimeoutMs(opts)};
    auto deadline = std::chrono::steady_clock::now() + timeout;
    auto pollScript =
        "window.__test.exists(" + jsStringLiteral(selector) + ")";

    while (true)
    {
        auto result = runJs(pollScript, {});
        if (result.isBool() && result.asBool())
            return true;
        if (std::chrono::steady_clock::now() >= deadline)
            throw std::runtime_error("AppDriver: waitFor timed out for "
                                     "selector: " + selector);
        std::this_thread::sleep_for(
            std::chrono::milliseconds {waitForPollMs});
    }
}

Miro::JSON AppDriver::evaluate(const std::string& expression, CallOptions opts)
{
    return runJs("window.__test.evaluate(" + jsStringLiteral(expression) + ")",
                 opts);
}

std::string AppDriver::dom(std::string_view selector, CallOptions opts)
{
    auto arg = selector.empty() ? std::string {"null"}
                                : jsStringLiteral(selector);
    return asString(runJs("window.__test.dom(" + arg + ")", opts));
}

ScreenshotResult AppDriver::screenshot(const ScreenshotOptions& options)
{
    auto callOpts = CallOptions {.timeoutMs = options.timeoutMs};
    auto bytes = runSnapshotBytes(callOpts);

    auto result = ScreenshotResult {};
    result.png = std::move(bytes);

    if (!options.path.empty())
    {
        writeBinary(options.path, result.png);
        result.path = options.path;
    }

    return result;
}

SnapshotResult AppDriver::snapshot(const std::string& name,
                                   const SnapshotOptions& options)
{
    auto baseName = sanitizeSnapshotName(name);
    auto baseDir = options.dir.empty() ? snapshotDir : options.dir;

    auto htmlPath = std::filesystem::path {baseDir} / (baseName + ".html");
    auto pngPath = std::filesystem::path {baseDir} / (baseName + ".png");

    auto callOpts = CallOptions {.timeoutMs = options.timeoutMs};

    auto html = dom(options.selector, callOpts);
    auto shot = screenshot({.timeoutMs = options.timeoutMs,
                            .path = pngPath.string()});

    writeText(htmlPath, html);

    return SnapshotResult {.name = name,
                           .dom = std::move(html),
                           .png = std::move(shot.png),
                           .domPath = htmlPath.string(),
                           .screenshotPath = pngPath.string()};
}

} // namespace eacp::WebView::Test
