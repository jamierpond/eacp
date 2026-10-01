#include <eacp/GPU/GPU.h>

using namespace eacp;

struct HelloWorldView final : GPU::GPUView
{
    void render(GPU::Frame& frame) override
    {
        auto pass = frame.beginPass({Graphics::Color {0.1f, 0.6f, 0.3f}});
        // fill me in: draw with pass
    }

    // fill me in: update(), touchBegan(), ...
};

int main()
{
    LOG("HelloWorld: hello from eacp"); // shows in adb logcat -s eacp
    return Graphics::runWindowedApp<HelloWorldView>();
}
