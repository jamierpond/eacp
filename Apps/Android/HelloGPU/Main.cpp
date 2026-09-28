#include <eacp/GPU/GPU.h>
#include <eacp/Graphics/Window/Android.h>
#include <eacp/Text/Text.h>

using namespace eacp;
using namespace GPU;
using namespace Maths;

// The Android example: a GPUView clearing through the Vulkan backend, a
// spinning triangle through the shader EDSL, a line of text through eacp's
// glyph atlas (rasterized by android.graphics), and every touch logged to
// logcat, with the clear colour following the first finger.

struct Color
{
    float r = 0.f;
    float g = 0.f;
    float b = 0.f;
};

struct Vertex
{
    Vec2 position;
    Color color;
};

EACP_SHADER_VALUE(Color, Float3)

namespace
{
const Vertex triangleVertices[] = {
    {{0.0f, 0.5f}, {1.0f, 0.2f, 0.2f}},
    {{-0.5f, -0.3f}, {0.2f, 1.0f, 0.2f}},
    {{0.5f, -0.3f}, {0.2f, 0.2f, 1.0f}},
};

struct TriangleShader final : ShaderProgram
{
    TriangleShader() { compile(); }

    void define() override
    {
        auto position = vertexInput(&Vertex::position);
        auto color = vertexInput(&Vertex::color);

        auto c = cos(angle);
        auto s = sin(angle);
        auto px = position.x();
        auto py = position.y();
        auto rotated = float2(px * c - py * s, (px * s + py * c) * aspect);

        setPosition(float4(rotated, 0.0f, 1.0f));
        setFragment(float4(varying(color), 1.0f));
    }

    Uniform<Float> angle;
    Uniform<Float> aspect;

    EACP_SHADER(angle, aspect)
};

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
        setContinuous(true);

        triangle.setVertices(triangleVertices);
        triangle.prepare(sampleCount());
        text.setSampleCount(sampleCount());

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
    }

    void update(Threads::FrameTime time) override
    {
        angle += radiansPerSecond * static_cast<float>(time.delta);
    }

    void render(Frame& frame) override
    {
        auto size = frame.logicalSize();
        auto pass = frame.beginPass({Graphics::Color {red, 0.45f, blue}});

        triangle.angle = angle;
        triangle.aspect = size.x / std::max(size.y, 1.f);
        pass.draw(triangle);

        drawText(pass, frame);
    }

    void drawText(RenderPass& pass, Frame& frame)
    {
        auto size = frame.logicalSize();
        auto title = Text::Font {"sans-serif", 28.f, Text::FontStyle::Bold};
        auto caption = Text::Font {"monospace", 15.f};
        auto shadow = Graphics::Color {0.f, 0.f, 0.f, 0.35f};
        auto white = Graphics::Color::white();

        text.setViewport(size, frame.backingScale());
        text.begin();

        auto centred = [&](std::string_view line, float y, const Text::Font& font)
        {
            auto x = (size.x - text.measure(line, font)) * 0.5f;
            text.draw(line, {x + 1.f, y + 1.f}, shadow, font);
            text.draw(line, {x, y}, white, font);
        };

        centred("Hello from eacp", size.y * 0.18f, title);
        centred("Vulkan + android.graphics text", size.y * 0.18f + 30.f, caption);
        centred("touch to change the colour", size.y * 0.85f, caption);

        text.flush(pass);
    }

    static constexpr float radiansPerSecond = 1.2f;

    TriangleShader triangle;
    Text::TextRenderer text;

    float angle = 0.f;
    float red = 0.95f;
    float blue = 0.35f;
};

int main()
{
    LOG("eacp HelloGPU: ",
        Device::shared().isValid() ? "Vulkan device up" : "no device");
    return Graphics::runWindowedApp<HelloView>();
}
