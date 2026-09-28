#pragma once

#include "../Primitives/Primitives.h"

// What an app may reach of the Android activity under eacp's window: the glue's
// android_app, every touch pointer (eacp's View events carry only the first),
// and the activity's pause and resume.

struct android_app;

namespace eacp::Graphics::Android
{
enum class TouchPhase
{
    Began,
    Moved,
    Ended,
    Cancelled
};

// One pointer's change, in the window's content points.
struct TouchEvent
{
    TouchPhase phase = TouchPhase::Began;
    int pointerId = 0;
    Point position;
};

// Null outside android_main.
android_app* getApp();

// Called for every pointer before the first one becomes a MouseEvent. Returning
// true keeps the event from the view tree.
void setTouchHandler(std::function<bool(const TouchEvent&)> handler);

// True on resume, false on pause.
void setLifecycleHandler(std::function<void(bool resumed)> handler);

// The window's content insets that system bars and cutouts cover, in points.
struct Insets
{
    float top = 0.f;
    float left = 0.f;
    float bottom = 0.f;
    float right = 0.f;
};

Insets getSafeAreaInsets();
} // namespace eacp::Graphics::Android
