#pragma once

#include "../Threads/EventLoop.h"
#include <ea_data_structures/Pointers/OwningPointer.h>
#include <string>

namespace eacp::Apps
{
struct AppBase
{
    virtual ~AppBase() = default;
};

template <typename T>
struct App : AppBase
{
    T app;
};

using AppHandle = EA::OwningPointer<AppBase>;

AppHandle& getGlobalApp();

void quit();

// Hands `url` off to the OS for its registered handler (e.g. the user's
// default browser for http/https). Useful for OAuth flows where the
// in-app WebView can't host the provider's login page and we need to
// escape to the system browser. Linux has no backend yet and will
// assert if called.
void openExternalURL(const std::string& url);

template <typename T>
void run()
{
    auto createFunc = [] { getGlobalApp().template create<App<T>>(); };
    Threads::runEventLoop(createFunc);
}

} // namespace eacp::Apps