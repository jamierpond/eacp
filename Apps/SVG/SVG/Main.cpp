#include <eacp/SVG/SVG.h>
#include <eacp/Graphics/Tray/TrayIcon.h>

using namespace eacp;

static constexpr std::string_view logoSvgMarkup()
{
    return R"SVG(
<svg width="585" height="510" viewBox="0 0 585 510" fill="none" xmlns="http://www.w3.org/2000/svg">
    <path d="M193.823 193.365V27H27V193.365" stroke="black" stroke-width="54"/>
    <path d="M376.159 509.48V426.527C376.159 380.46 338.814 343.115 292.747 343.115C246.681 343.115 209.336 380.46 209.336 426.527V509.48" stroke="black" stroke-width="54"/>
    <path d="M558 193.365V27H391.177V193.365" stroke="black" stroke-width="54"/>
    <path d="M197.445 13.622L292.413 182.393L387.709 13.622" stroke="black" stroke-width="54" stroke-linejoin="bevel"/>
    <path d="M193.723 192.984C193.723 247.439 237.979 291.584 292.571 291.584C347.163 291.584 391.177 247.687 391.177 193.231" stroke="black" stroke-width="54"/>
</svg>
)SVG";
}

// The same markup rasterized through SVG::toImage at a run of sizes and
// painted with Context::drawImage — the raster counterpart of the live
// SVGView above it.
struct RasterStrip final : Graphics::View
{
    void setMarkup(const std::string& markup)
    {
        images.clear();
        for (auto size: {16, 32, 64, 128})
            images.add(SVG::toImage(markup, size));

        repaint();
    }

    void paint(Graphics::Context& context) override
    {
        auto bounds = getLocalBounds();
        context.setColor(Graphics::Color::gray(0.93f));
        context.fillRect(bounds);

        auto x = 16.f;
        for (auto& image: images)
        {
            auto w = static_cast<float>(image.width());
            auto h = static_cast<float>(image.height());
            context.drawImage(image, {x, (bounds.h - h) / 2.f, w, h});
            x += w + 16.f;
        }
    }

    Vector<Graphics::Image> images;
};

struct MainView final : Graphics::View
{
    void resized() override
    {
        auto bounds = getLocalBounds();
        auto stripBounds = bounds.removeFromBottom(160.f);

        if (vectorView != nullptr)
            vectorView->setBounds(bounds);
        strip.setBounds(stripBounds);
    }

    Graphics::View* vectorView = nullptr;
    RasterStrip strip;
};

struct MyApp
{
    MyApp()
    {
        auto path = Files::getBundleResourcePath("example.svg");
        auto contents = Files::readFile(path);
        result = SVG::parse(contents);
        if (result.root)
        {
            result.root->stretchToFit();
            main.vectorView = result.root;
            main.addSubview(*result.root);
        }

        main.strip.setMarkup(contents);
        main.addSubview(main.strip);
        window.setContentView(main);

        trayIcon.setIcon(SVG::toImage(std::string {logoSvgMarkup()}, 36, 36));
        trayIcon.setTooltip("SVG App");
    }

    SVG::ParseResult result;
    MainView main;
    Graphics::Window window;
    Graphics::TrayIcon trayIcon;
};

int main()
{
    Apps::run<MyApp>();

    return 0;
}
