#include "Android.h"
#include "Window.h"

#include "../Graphics/Keyboard.h"
#include "../View/AndroidViewSurface-Android.h"

#include <eacp/Core/App/App.h>
#include <eacp/Core/Threads/EventLoop-Android.h>
#include <eacp/Core/Utils/Environment.h>

#include <android/configuration.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>

#include <cmath>
#include <cstdlib>
#include <string>

namespace eacp::Graphics
{
namespace
{
struct AndroidPrimaryTouch
{
    int pointerId = -1;
    Point downPosition;
};

// The activity as the glue reports it, outliving any one Window: the native
// window can arrive before the app has built one.
struct AndroidWindow;

struct AndroidActivity
{
    android_app* app = nullptr;
    AndroidWindow* window = nullptr;

    std::function<bool(const Android::TouchEvent&)> touchHandler =
        [](const Android::TouchEvent&) { return false; };
    std::function<void(bool)> lifecycleHandler = [](bool) {};

    AndroidPrimaryTouch primary;
};

AndroidActivity& androidActivity()
{
    static auto activity = AndroidActivity {};
    return activity;
}

float androidBackingScale(android_app* app)
{
    if (app == nullptr || app->config == nullptr)
        return 1.f;

    auto density = AConfiguration_getDensity(app->config);

    if (density <= 0 || density == ACONFIGURATION_DENSITY_ANY
        || density == ACONFIGURATION_DENSITY_NONE)
        return 1.f;

    return (float) density / (float) ACONFIGURATION_DENSITY_MEDIUM;
}

void androidHandleCommand(android_app* app, int32_t command);
int32_t androidHandleInput(android_app* app, AInputEvent* event);

// $HOME and the XDG roots point into the app's own storage, so the Linux
// FilePath code and the Vulkan pipeline cache find writable directories.
void androidPointHomeAtAppStorage(android_app* app)
{
    const auto* activity = app->activity;

    if (activity == nullptr || activity->internalDataPath == nullptr)
        return;

    auto data = std::string {activity->internalDataPath};

    setEnv("HOME", data);
    setEnv("XDG_CONFIG_HOME", data + "/config");
    setEnv("XDG_DATA_HOME", data + "/data");
    setEnv("XDG_CACHE_HOME", data + "/cache");
}
struct AndroidWindow : AndroidWindowSurface
{
    AndroidWindow(const WindowOptions& optionsToUse, WindowEvents& eventsToUse)
        : quitCallback(optionsToUse.effectiveOnQuit())
        , onResize(optionsToUse.onResize)
        , events(&eventsToUse)
    {
        auto& activity = androidActivity();
        activity.window = this;

        refresh(activity.app);
    }

    ~AndroidWindow()
    {
        if (contentView != nullptr)
            androidUnbindWindowFromContentView(*contentView);

        auto& activity = androidActivity();

        if (activity.window == this)
            activity.window = nullptr;
    }

    // Reads the native window and density afresh; true when the size changed.
    bool refresh(android_app* app)
    {
        auto* current = app != nullptr ? app->window : nullptr;
        auto oldSize = contentSize;

        nativeWindow = current;
        scale = androidBackingScale(app);

        if (nativeWindow != nullptr)
        {
            pixelWidth = ANativeWindow_getWidth(nativeWindow);
            pixelHeight = ANativeWindow_getHeight(nativeWindow);
            contentSize = {(float) pixelWidth / scale, (float) pixelHeight / scale};
        }
        else
        {
            pixelWidth = 0;
            pixelHeight = 0;
        }

        if (app != nullptr && nativeWindow != nullptr)
            readInsets(app->contentRect);

        return contentSize.x != oldSize.x || contentSize.y != oldSize.y;
    }

    // The glue's content rect is the part of the window no system bar covers.
    void readInsets(const ARect& rect)
    {
        if (rect.right <= rect.left || rect.bottom <= rect.top)
        {
            insets = {};
            return;
        }

        insets.top = (float) rect.top / scale;
        insets.left = (float) rect.left / scale;
        insets.bottom = (float) std::max(pixelHeight - rect.bottom, 0) / scale;
        insets.right = (float) std::max(pixelWidth - rect.right, 0) / scale;
    }

    void surfaceChanged(android_app* app)
    {
        auto resized = refresh(app);

        if (contentView == nullptr)
            return;

        if (resized)
            layOutContent();

        androidWindowSurfaceChanged(*contentView);
    }

    void layOutContent()
    {
        if (contentView == nullptr)
            return;

        contentView->setBounds({0.f, 0.f, contentSize.x, contentSize.y});

        if (onResize)
            onResize((int) contentSize.x, (int) contentSize.y);
    }

    void setContentView(View* view)
    {
        if (contentView != nullptr)
            androidUnbindWindowFromContentView(*contentView);

        contentView = view;

        if (contentView == nullptr)
            return;

        layOutContent();
        androidBindWindowToContentView(*contentView, *this);
    }

    void dispatchPrimary(MouseEventType type, Point position, int64_t eventTime)
    {
        if (contentView == nullptr)
            return;

        auto& primary = androidActivity().primary;

        if (type == MouseEventType::Down)
            primary.downPosition = position;

        auto event = MouseEvent {};
        event.type = type;
        event.pos = position;
        event.downPos = primary.downPosition;
        event.button = MouseButton::Left;
        event.timestamp = (double) eventTime / 1e9;

        if (type == MouseEventType::Down || type == MouseEventType::Up)
            event.clickCount = 1;

        contentView->dispatchMouseEvent(event);
    }

    Callback quitCallback;
    ResizeCallback onResize;
    WindowEvents* events;

    Point contentSize {640.f, 400.f};
    Android::Insets insets;
    bool focused = false;
};

void androidDispatchPointer(AndroidWindow* window,
                            Android::TouchPhase phase,
                            const AInputEvent* event,
                            size_t index)
{
    const auto scale = window != nullptr ? window->scale : 1.f;
    const auto id = (int) AMotionEvent_getPointerId(event, index);
    const auto position = Point {AMotionEvent_getX(event, index) / scale,
                                 AMotionEvent_getY(event, index) / scale};

    auto& activity = androidActivity();

    if (activity.touchHandler({phase, id, position}))
        return;

    if (window == nullptr)
        return;

    auto& primary = activity.primary;
    const auto time = AMotionEvent_getEventTime(event);

    switch (phase)
    {
        case Android::TouchPhase::Began:
            if (primary.pointerId >= 0)
                return;

            primary.pointerId = id;
            window->dispatchPrimary(MouseEventType::Down, position, time);
            return;

        case Android::TouchPhase::Moved:
            if (primary.pointerId == id)
                window->dispatchPrimary(MouseEventType::Dragged, position, time);
            return;

        case Android::TouchPhase::Ended:
        case Android::TouchPhase::Cancelled:
            if (primary.pointerId != id)
                return;

            primary.pointerId = -1;
            window->dispatchPrimary(MouseEventType::Up, position, time);
            return;
    }
}

int32_t androidHandleMotion(AInputEvent* event)
{
    auto* window = androidActivity().window;
    const auto action = AMotionEvent_getAction(event);
    const auto masked = action & AMOTION_EVENT_ACTION_MASK;
    const auto index = (size_t) ((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                                 >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);

    switch (masked)
    {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            androidDispatchPointer(window, Android::TouchPhase::Began, event, index);
            return 1;

        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
            androidDispatchPointer(window, Android::TouchPhase::Ended, event, index);
            return 1;

        case AMOTION_EVENT_ACTION_CANCEL:
            for (auto i = size_t {0}; i < AMotionEvent_getPointerCount(event); ++i)
                androidDispatchPointer(
                    window, Android::TouchPhase::Cancelled, event, i);
            return 1;

        case AMOTION_EVENT_ACTION_MOVE:
            for (auto i = size_t {0}; i < AMotionEvent_getPointerCount(event); ++i)
                androidDispatchPointer(window, Android::TouchPhase::Moved, event, i);
            return 1;

        default:
            return 0;
    }
}

// Back arrives as Escape, which is what a desktop app already treats as leave.
int32_t androidHandleKey(AInputEvent* event)
{
    if (AKeyEvent_getKeyCode(event) != AKEYCODE_BACK)
        return 0;

    auto* window = androidActivity().window;

    if (window == nullptr || window->contentView == nullptr)
        return 1;

    auto key = KeyEvent {};
    key.keyCode = KeyCode::Escape;
    key.timestamp = (double) AKeyEvent_getEventTime(event) / 1e9;

    if (AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_DOWN)
    {
        key.type = KeyEventType::Down;
        key.isRepeat = AKeyEvent_getRepeatCount(event) > 0;
        window->contentView->keyDown(key);
    }
    else if (AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_UP)
    {
        key.type = KeyEventType::Up;
        window->contentView->keyUp(key);
    }

    return 1;
}

int32_t androidHandleInput(android_app*, AInputEvent* event)
{
    switch (AInputEvent_getType(event))
    {
        case AINPUT_EVENT_TYPE_MOTION:
            return androidHandleMotion(event);

        case AINPUT_EVENT_TYPE_KEY:
            return androidHandleKey(event);

        default:
            return 0;
    }
}

// Runs between the glue's pre- and post-command steps, so a TERM_WINDOW has
// dropped the swapchain before the glue lets the surface go.
void androidHandleCommand(android_app* app, int32_t command)
{
    auto& activity = androidActivity();
    auto* window = activity.window;

    switch (command)
    {
        case APP_CMD_INIT_WINDOW:
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
        case APP_CMD_CONTENT_RECT_CHANGED:
            if (window != nullptr)
                window->surfaceChanged(app);
            break;

        // The glue still holds the window here; the record must let go of it.
        case APP_CMD_TERM_WINDOW:
            if (window != nullptr)
            {
                window->nativeWindow = nullptr;

                if (window->contentView != nullptr)
                    androidWindowSurfaceChanged(*window->contentView);
            }
            break;

        case APP_CMD_GAINED_FOCUS:
        case APP_CMD_LOST_FOCUS:
            if (window != nullptr)
            {
                window->focused = command == APP_CMD_GAINED_FOCUS;
                window->events->onActivationChanged(window->focused);
            }
            break;

        case APP_CMD_RESUME:
            activity.lifecycleHandler(true);
            break;

        case APP_CMD_PAUSE:
            activity.lifecycleHandler(false);
            break;

        case APP_CMD_DESTROY:
            Apps::quit();
            break;

        default:
            break;
    }
}

void androidHandleLooperEvent(int ident, void* data)
{
    auto* app = androidActivity().app;

    if (app == nullptr || data == nullptr)
        return;

    if (ident == LOOPER_ID_MAIN || ident == LOOPER_ID_INPUT)
    {
        auto* source = static_cast<android_poll_source*>(data);
        source->process(app, source);
    }
}
} // namespace

struct Window::Native : AndroidWindow
{
    using AndroidWindow::AndroidWindow;
};

namespace Android
{
android_app* getApp()
{
    return androidActivity().app;
}

void setTouchHandler(std::function<bool(const TouchEvent&)> handler)
{
    androidActivity().touchHandler =
        handler ? std::move(handler)
                : std::function<bool(const TouchEvent&)> {[](const TouchEvent&)
                                                          { return false; }};
}

void setLifecycleHandler(std::function<void(bool resumed)> handler)
{
    androidActivity().lifecycleHandler =
        handler ? std::move(handler) : std::function<void(bool)> {[](bool) {}};
}

Insets getSafeAreaInsets()
{
    auto* window = androidActivity().window;
    return window != nullptr ? window->insets : Insets {};
}
} // namespace Android

Window::Window(const WindowOptions& optionsToUse)
    : options(optionsToUse)
    , impl(optionsToUse, events)
{
}

Window::~Window() = default;

// An activity's title is the manifest's label.
void Window::setTitle(const std::string&) {}

void* Window::getHandle()
{
    return impl->nativeWindow;
}

void* Window::getContentViewHandle()
{
    return impl->contentView != nullptr ? impl->contentView->getHandle() : nullptr;
}

void Window::setContentView(View& view)
{
    contentLink.attach(&view, this);
    impl->setContentView(&view);
}

// One full-screen window per activity: nothing to raise, hide, move or size.
void Window::toFront() {}

void Window::setVisible(bool) {}

bool Window::isVisible()
{
    return impl->nativeWindow != nullptr;
}

Point Window::getPosition() const
{
    return {};
}

void Window::setPosition(Point) {}

Point Window::getSize() const
{
    return impl->contentSize;
}

void Window::setSize(Point) {}

void Window::minimize() {}

void Window::toggleMaximize() {}

void Window::setMouseLocked(bool) {}

bool Window::isMouseLocked() const
{
    return false;
}

bool Window::isKeyPressed(uint16_t virtualKeyCode) const
{
    return Keyboard::isKeyPressed(virtualKeyCode);
}

bool Window::isShiftPressed() const
{
    return Keyboard::isShiftPressed();
}

bool Window::isControlPressed() const
{
    return Keyboard::isControlPressed();
}

bool Window::isAltPressed() const
{
    return Keyboard::isAltPressed();
}

bool Window::isCommandPressed() const
{
    return Keyboard::isCommandPressed();
}

ModifierKeys Window::getModifiers() const
{
    return Keyboard::getModifiers();
}

} // namespace eacp::Graphics

// Called by android_main (AndroidMain-Android.c) around the app's main().
extern "C" void eacpAndroidStart(android_app* app)
{
    using namespace eacp::Graphics;

    androidActivity().app = app;
    androidPointHomeAtAppStorage(app);

    app->onAppCmd = androidHandleCommand;
    app->onInputEvent = androidHandleInput;

    eacp::Threads::setLooperEventHandler(androidHandleLooperEvent);
}

extern "C" void eacpAndroidFinish(android_app* app)
{
    ANativeActivity_finish(app->activity);
    eacp::Graphics::androidActivity().app = nullptr;
}
