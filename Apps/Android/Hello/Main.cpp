#include <eacp/GPU/GPU.h>
#include <eacp/Graphics/Window/Android.h>
#include <eacp/Text/Text.h>

using namespace eacp;
using namespace GPU;

// The smallest Android check: a GPUView clearing through the Vulkan backend,
// every touch pointer logged, the colour following the first finger, and lines
// of text through eacp's glyph atlas on top.

namespace
{
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

    float red = 0.95f;
    float blue = 0.35f;
};

int main()
{
    LOG("eacp Hello: ",
        Device::shared().isValid() ? "Vulkan device up" : "no device");
    return Graphics::runWindowedApp<HelloView>();
}
