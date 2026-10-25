#pragma once

#include <eacp/Core/Plugins/Reloadable.h>
#include <eacp/GPU/GPU.h>

struct FullScreenProgram;

// Defined in Views.cpp, which the module recompiles, and held through a
// Plugins::Reloadable: each reload points this view's vtable at the fresh image, so update and render run the new code, and
// onReload remakes the shader from it. Only method bodies may change while the
// app runs; this header's layout is the running app's.
struct DemoView : eacp::GPU::GPUView
{
    DemoView();
    ~DemoView() override;

    void update(eacp::Threads::FrameTime frameTime) override;
    void render(eacp::GPU::Frame& frame) override;
    EACP_RELOADABLE void onReload();

    eacp::OwningPointer<FullScreenProgram> shader;
    float elapsed = 0.f;
};
