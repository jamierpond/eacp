#pragma once

#include "../Threads/EventLoop.h"
#include "../Utils/Common.h"
#include <ea_data_structures/Pointers/OwningPointer.h>
#include <optional>
#include <string>
#include <vector>

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
using AppFactory = Callback;

AppHandle& getGlobalApp();
AppFactory& getAppFactory();

void quit();

// Destroys the current app instance and recreates it via the factory
// captured by run<T>(). Marshaled onto the message thread so it's
// safe to call from any thread. Returns immediately — the destroy +
// recreate happens on the next runloop tick.
void restart();

// Hands `url` off to the OS for its registered handler (e.g. the user's
// default browser for http/https). Useful for OAuth flows where the
// in-app WebView can't host the provider's login page and we need to
// escape to the system browser. Linux has no backend yet and will
// assert if called.
void openExternalURL(const std::string& url);

struct FilePickerOptions
{
    std::vector<std::string> allowedExtensions;
};

// Shows the OS's native file chooser, blocking until the user picks a file
// or cancels. Returns the chosen absolute path, or std::nullopt on cancel.
// Must be called on the UI thread. Implemented on macOS; other backends
// return std::nullopt for now.
std::optional<std::string> chooseFile(const FilePickerOptions& options = {});

// Shows the OS's native folder chooser, blocking until the user picks a
// directory or cancels. Returns the chosen absolute path, or std::nullopt
// on cancel. Must be called on the UI thread. Implemented on macOS; other
// backends return std::nullopt for now.
std::optional<std::string> chooseDirectory();

template <typename T>
void run()
{
    auto createFunc = [] { getGlobalApp().template create<App<T>>(); };
    getAppFactory() = createFunc;
    Threads::runEventLoop(createFunc);
}

} // namespace eacp::Apps