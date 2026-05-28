#pragma once

#include "AppDriver.h"
#include "TestAgent.h"

#include <memory>
#include <optional>

namespace eacp::WebView::Test
{

// TestApp<T> owns a live instance of the user's app struct + the
// AppDriver that drives it. Construct one per test on the main
// thread; its destructor cleans up. The contained T must expose:
//
//   * `Graphics::WebView webView`   - the view tests drive
//   * `Graphics::WebViewBridge transport` - to reach Miro::Bridge
//
// These match the convention used by the SOURCES-/API-style apps
// produced by eacp_add_webview_app(). Example:
//
//   auto tFoo = test("MyApp/foo") = [] {
//       auto app = TestApp<MyApp> {};
//       app.driver().waitFor("...");
//       check(app.driver().count("...") == 3);
//   };
//
// The driver injects the window.__test JS agent into the WebView
// before the first navigation finishes, so test commands work as
// soon as the page has rendered.
template <typename T>
struct TestApp
{
    TestApp()
    {
        instance.create();
        instance->webView.addUserScript(loadTestAgentSource(), true);
        driverImpl.emplace(instance->webView,
                           instance->transport.getBridge());
    }

    T& app() { return *instance; }
    const T& app() const { return *instance; }

    AppDriver& driver() { return *driverImpl; }
    const AppDriver& driver() const { return *driverImpl; }

private:

    EA::OwningPointer<T> instance;
    std::optional<AppDriver> driverImpl;
};

} // namespace eacp::WebView::Test
