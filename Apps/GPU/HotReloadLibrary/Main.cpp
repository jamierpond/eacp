#include "DemoView.h"

#include <string>

// The same as Apps/GPU/HotReload except that Views.cpp and Tuning.cpp belong
// to HotReloadLibraryViews, a static library the app links and other targets
// could link too. Under EACP_HOT_RELOAD eacp_add_hot_reload's LIBRARY form
// builds both files as a module, HotReloadLibraryView, with the library's
// flags: nothing is compiled twice into one binary. Save either file and the
// module reloads: DemoView's vtable moves to the fresh image and its onReload()
// remakes the shader, whose define() calls the fresh getPlasmaSpeed().

using namespace eacp;

namespace
{
struct DemoRoot final : Graphics::View
{
    DemoRoot()
    {
        addSubview(*view);
        reloadedModule.onStateChanged = [this](const std::string& state)
        {
            Console::print(state);
            showStatus();
        };
    }

    void resized() override { view->setBounds(getLocalBounds()); }

    void showStatus() { onStatusChanged(reloadedModule.describeState()); }

    std::function<void(const std::string& status)> onStatusChanged =
        [](const std::string&) {};

    Plugins::HotReloadedModule reloadedModule {Plugins::hotReloadTarget()};
    Plugins::Reloadable<DemoView> view;
};

Graphics::WindowOptions makeOptions()
{
    auto options = Graphics::WindowOptions {};
    options.width = 960;
    options.height = 600;
    options.title = "eacp GPU — hot-reloaded library";
    options.minWidth = 320;
    options.minHeight = 240;
    return options;
}

struct DemoApp
{
    DemoApp()
    {
        auto showInTitle = [this](const std::string& status)
        { window.setTitle(makeOptions().title + " — " + status); };

        root.onStatusChanged = showInTitle;
        root.showStatus();
    }

    DemoRoot root;
    Graphics::Window window {root, makeOptions()};
};
} // namespace

int main()
{
    return Apps::run<DemoApp>();
}
