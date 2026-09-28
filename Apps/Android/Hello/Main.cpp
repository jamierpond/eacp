#include <eacp/GPU/GPU.h>
#include <eacp/Graphics/Window/Android.h>

using namespace eacp;
using namespace GPU;

// The smallest Android check: a GPUView clearing through the Vulkan backend,
// every touch pointer logged, and the colour following the first finger.

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
        (void) pass;
    }

    float red = 0.95f;
    float blue = 0.35f;
};

int main()
{
    LOG("eacp Hello: ",
        Device::shared().isValid() ? "Vulkan device up" : "no device");
    return Graphics::runWindowedApp<HelloView>();
}
