#pragma once

#include "../Primitives/Primitives.h"

struct ANativeWindow;

// What Window-Android.cpp needs of View-Android.cpp.

namespace eacp::Graphics
{
class View;

// An activity has one surface, so a window's is handed whole to the first
// presenting view in its tree.
struct AndroidWindowSurface
{
    ANativeWindow* nativeWindow = nullptr;
    int pixelWidth = 0;
    int pixelHeight = 0;
    float scale = 1.f;
    View* contentView = nullptr;
};

void androidBindWindowToContentView(View& contentView, AndroidWindowSurface& window);
void androidUnbindWindowFromContentView(View& contentView);

// The native window came, went or changed size.
void androidWindowSurfaceChanged(View& contentView);
} // namespace eacp::Graphics
