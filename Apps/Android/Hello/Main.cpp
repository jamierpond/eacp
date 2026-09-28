#include <eacp/GPU/GPU.h>
#include <eacp/Graphics/Window/Android.h>
#include <eacp/Sprites/Sprites.h>
#include <eacp/Text/Text.h>

using namespace eacp;
using namespace GPU;

// The smallest Android check: a GPUView clearing through the Vulkan backend,
// every touch pointer logged, the colour following the first finger, and a
// SoftwareContext overlay of rings, a disc and text drawn as a sprite on top,
// and lines of text through eacp's glyph atlas under it.

namespace
{
Graphics::Image paintOverlay(Graphics::Point size, float scale)
{
    auto context = Graphics::SoftwareContext(
        int(std::lround(size.x * scale)), int(std::lround(size.y * scale)), scale);

    auto ring = Graphics::Path();
    ring.addEllipse({40.f, 120.f, 120.f, 120.f});
    context.setColor(Graphics::Color::white(0.85f));
    context.setLineWidth(2.5f);
    context.strokePath(ring);

    auto thickRing = Graphics::Path();
    thickRing.addEllipse({190.f, 120.f, 120.f, 120.f});
    context.setLineWidth(5.f);
    context.strokePath(thickRing);

    auto disc = Graphics::Path();
    disc.addEllipse({70.f, 150.f, 60.f, 60.f});
    context.setColor({0.1f, 0.1f, 0.2f, 0.6f});
    context.fillPath(disc);

    context.setColor(Graphics::Color::white(0.3f));
    context.fillRoundedRect({40.f, 270.f, 270.f, 70.f}, 14.f);

    auto regular =
        Graphics::Font(Graphics::FontOptions().withName("Menlo").withSize(17.f));
    auto bold = Graphics::Font(
        Graphics::FontOptions().withName("Menlo-Bold").withSize(17.f));
    auto proportional = Graphics::Font(Graphics::FontOptions().withSize(14.f));

    context.setColor(Graphics::Color::white());
    context.drawText("Moo Jump", {56.f, 300.f}, regular);
    context.drawText("Moo Jump", {176.f, 300.f}, bold);
    context.drawText("q / Esc to quit - Again", {56.f, 326.f}, proportional);

    LOG("overlay: \"Moo Jump\" is ",
        Graphics::TextMetrics::measureWidth("Moo Jump", regular),
        "pt wide, ascent ",
        Graphics::TextMetrics::getAscent(regular));

    return context.getImage();
}

const char* phaseName(Graphics::Android::TouchPhase phase)
{
    switch (phase)
    {
        case Graphics::Android::TouchPhase::Began:
            return "began";
        case Graphics::Android::TouchPhase::Moved:
            return "moved";
        case Graphics::Android::TouchPhase::Ended:
            return "ended";
        case Graphics::Android::TouchPhase::Cancelled:
            return "cancelled";
    }

    return "";
}
} // namespace

struct HelloView final : GPUView
{
    HelloView()
    {
        setHandlesMouseEvents(true);

        Graphics::Android::setTouchHandler(
            [](const Graphics::Android::TouchEvent& touch)
            {
                if (touch.phase != Graphics::Android::TouchPhase::Moved)
                    LOG("touch ",
                        touch.pointerId,
                        " ",
                        phaseName(touch.phase),
                        " at ",
                        touch.position.x,
                        ", ",
                        touch.position.y);

                return false;
            });
    }

    void mouseDown(const Graphics::MouseEvent& event) override { follow(event); }
    void mouseDragged(const Graphics::MouseEvent& event) override { follow(event); }

    void follow(const Graphics::MouseEvent& event)
    {
        auto bounds = getLocalBounds();
        red = std::clamp(event.pos.x / std::max(bounds.w, 1.f), 0.f, 1.f);
        blue = std::clamp(event.pos.y / std::max(bounds.h, 1.f), 0.f, 1.f);
        repaint();
    }

    void render(Frame& frame) override
    {
        auto pass = frame.beginPass({Graphics::Color {red, 0.45f, blue}});
        auto size = getLocalBounds();

        if (!overlay || overlaySize.x != size.w || overlaySize.y != size.h)
        {
            overlaySize = {size.w, size.h};
            overlay = Device::shared().makeTexture(
                paintOverlay(overlaySize, backingScale()));
            sprites.setLogicalSize(overlaySize);
        }

        sprites.begin(pass);
        sprites.drawTexture(*overlay, {0.f, 0.f, size.w, size.h});
        sprites.end();

        drawText(pass, frame);
    }

    void drawText(RenderPass& pass, Frame& frame)
    {
        auto regular = Text::Font {"Menlo", 17.f};
        auto bold = Text::Font {"Menlo", 17.f, Text::FontStyle::Bold};
        auto sans = Text::Font {"sans-serif", 22.f};
        auto shadow = Graphics::Color {0.f, 0.12f, 0.f, 0.3f};
        auto white = Graphics::Color::white(0.9f);

        text.setViewport(frame.logicalSize(), frame.backingScale());
        text.setSampleCount(sampleCount());
        text.begin();

        auto line = [&](std::string_view string, float y, const Text::Font& font)
        {
            text.draw(string, {41.f, y + 1.f}, shadow, font);
            text.draw(string, {40.f, y}, white, font);
        };

        line("Moo Jump Again", 400.f, regular);
        line("Moo Jump Again", 430.f, bold);
        line("wasd / hjkl to walk  -  q to quit", 460.f, regular);
        line("Glyphs by android.graphics", 500.f, sans);
        line("gjpqy Ag 0123456789 {}[]()", 535.f, sans);

        text.flush(pass);
    }

    Text::TextRenderer text;

    Sprites::SpriteRenderer sprites {{1.f, 1.f}, sampleCount()};
    std::optional<Texture> overlay;
    Graphics::Point overlaySize;

    float red = 0.95f;
    float blue = 0.35f;
};

int main()
{
    LOG("eacp Hello: ",
        Device::shared().isValid() ? "Vulkan device up" : "no device");
    return Graphics::runWindowedApp<HelloView>();
}
