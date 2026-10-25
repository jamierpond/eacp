#include "DemoView.h"

#include <string>

// A window filled by DemoView, a GPUView defined in Views.cpp beside this file
// and held here by value. Under EACP_HOT_RELOAD, Views.cpp is also built as a
// module, HotReloadGPUView, bound at load time to this app's own eacp: change
// a shader's tuning, or which shader makeDemoShader returns, and save. The
// module is rebuilt and loaded, the view's vtable pointer is moved to the
// fresh image's DemoView vtable, and its onReload() remakes the shader there.
// The view keeps its state; only Views.cpp's function bodies may change while
// the app runs (a changed header is refused).

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
    options.title = "eacp GPU — hot-reloaded view";
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
