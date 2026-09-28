#include "View-Linux.h"

#include "AndroidViewSurface-Android.h"
#include "View.h"
#include "../Image/Image.h"

#include <eacp/Core/Threads/Async.h>
#include <eacp/Core/Threads/EventLoop.h>

#include <android/choreographer.h>

#include <memory>
#include <unordered_map>

// View-Linux.cpp's view tree without subsurfaces: Android gives an activity one
// surface, and the first presenting view in the window's tree draws into it.

namespace eacp::Graphics
{
namespace
{
struct AndroidViewRecord
{
    ViewSurface record;
    View* view = nullptr;
    bool repaintPending = false;
};

using AndroidViewRecordPtr = std::shared_ptr<AndroidViewRecord>;

std::unordered_map<View*, AndroidViewRecordPtr>& androidViewRecords()
{
    static auto records = std::unordered_map<View*, AndroidViewRecordPtr> {};
    return records;
}

std::unordered_map<View*, AndroidWindowSurface*>& androidContentViewWindows()
{
    static auto windows = std::unordered_map<View*, AndroidWindowSurface*> {};
    return windows;
}

AndroidViewRecordPtr androidFindViewRecord(View& view)
{
    auto& records = androidViewRecords();
    auto found = records.find(&view);

    return found == records.end() ? AndroidViewRecordPtr {} : found->second;
}

View& androidRootOf(View& view)
{
    auto* root = &view;

    while (root->getParent() != nullptr)
        root = root->getParent();

    return *root;
}

AndroidWindowSurface* androidWindowForView(View& view)
{
    auto& windows = androidContentViewWindows();
    auto found = windows.find(&androidRootOf(view));

    return found == windows.end() ? nullptr : found->second;
}

View* androidFirstPresentingView(View& view)
{
    if (!view.isVisible())
        return nullptr;

    if (androidFindViewRecord(view) != nullptr)
        return &view;

    for (auto* child: view.getSubviews())
        if (auto* found = androidFirstPresentingView(*child))
            return found;

    return nullptr;
}

void androidLoseSurface(AndroidViewRecord& state)
{
    if (state.record.surface == nullptr)
        return;

    state.record.onLost();
    state.record.surface = nullptr;
    state.record.pixelWidth = 0;
    state.record.pixelHeight = 0;
    state.record.frameCallbackPending = false;
}

void androidSyncRecord(AndroidViewRecord& state,
                       AndroidWindowSurface* window,
                       bool chosen)
{
    auto* wanted = chosen && window != nullptr ? window->nativeWindow : nullptr;

    if (state.record.surface != wanted)
    {
        androidLoseSurface(state);

        if (wanted == nullptr)
            return;

        state.record.surface = wanted;
        state.record.pixelWidth = window->pixelWidth;
        state.record.pixelHeight = window->pixelHeight;
        state.record.scale = window->scale;
        state.record.onAvailable();
        return;
    }

    if (wanted == nullptr)
        return;

    if (state.record.pixelWidth != window->pixelWidth
        || state.record.pixelHeight != window->pixelHeight
        || state.record.scale != window->scale)
    {
        state.record.pixelWidth = window->pixelWidth;
        state.record.pixelHeight = window->pixelHeight;
        state.record.scale = window->scale;
        state.record.onResized();
    }
}

// Every presenting view under `root`'s window, with the one that owns the
// surface decided afresh.
void androidSyncViewSurfaces(View& anyView)
{
    auto& root = androidRootOf(anyView);
    auto* window = androidWindowForView(root);
    auto* chosen = window != nullptr ? androidFirstPresentingView(root) : nullptr;

    auto states = Vector<AndroidViewRecordPtr> {};

    for (auto& [view, state]: androidViewRecords())
        if (&androidRootOf(*view) == &root)
            states.add(state);

    for (auto& state: states)
        androidSyncRecord(*state, window, state->view == chosen);
}

void androidReleaseViewSurfaceTree(View& view)
{
    if (auto state = androidFindViewRecord(view))
        androidLoseSurface(*state);

    for (auto* child: view.getSubviews())
        androidReleaseViewSurfaceTree(*child);
}

struct AndroidFrameRequest
{
    std::weak_ptr<AndroidViewRecord> record;
};

void androidFrameArrived(int64_t, void* data)
{
    auto request = std::unique_ptr<AndroidFrameRequest>(
        static_cast<AndroidFrameRequest*>(data));

    auto state = request->record.lock();

    if (state == nullptr)
        return;

    state->record.frameCallbackPending = false;
    state->record.onFrameDone();
}
} // namespace

struct View::Native
{
    explicit Native(View* owner)
        : ownerView(owner)
    {
    }

    Rect getBounds() const { return bounds; }

    void setBounds(const Rect& newBounds)
    {
        bounds = newBounds;
        ownerView->resizeStarted();
        ownerView->resized();
        ownerView->resizeFinished();
    }

    void focus() { focused = true; }
    bool hasFocus() const { return focused; }

    View* ownerView;
    Rect bounds;
    bool focused = false;
};

View::View()
    : impl(this)
{
}

View::~View()
{
    if (auto state = androidFindViewRecord(*this))
        androidLoseSurface(*state);

    androidViewRecords().erase(this);

    auto& windows = androidContentViewWindows();
    auto owner = windows.find(this);

    if (owner != windows.end())
    {
        owner->second->contentView = nullptr;
        windows.erase(owner);
    }

    for (auto* layer: getLayers())
        layer->detachFromLayer();

    removeFromParent();
}

void* View::getHandle()
{
    return impl.get();
}

void* View::getNativeLayer()
{
    return impl.get();
}

void View::repaint()
{
    auto state = androidFindViewRecord(*this);

    if (state == nullptr || state->repaintPending)
        return;

    state->repaintPending = true;

    Threads::callAsync(
        [weak = std::weak_ptr<AndroidViewRecord>(state)]
        {
            auto pending = weak.lock();

            if (pending == nullptr)
                return;

            pending->repaintPending = false;

            if (pending->record.surface != nullptr)
                pending->record.onRepaint();
        });
}

void View::setOpacity(float opacityToUse)
{
    opacity = opacityToUse;
}

void View::setVisible(bool shouldBeVisible)
{
    if (visible == shouldBeVisible)
        return;

    visible = shouldBeVisible;
    notifyVisibilityChanged(shouldBeVisible);

    androidSyncViewSurfaces(*this);
}

Rect View::getBounds() const
{
    return impl->getBounds();
}

void View::setBounds(const Rect& bounds)
{
    impl->setBounds(bounds);
}

// A touch screen has no pointer between touches.
Point View::getMousePosition() const
{
    return {};
}

void View::setMouseCursor(MouseCursor cursor)
{
    currentCursor = cursor;
}

void View::focus()
{
    impl->focus();
}

bool View::hasFocus() const
{
    return impl->hasFocus();
}

void notifyBackingScaleChanged(View& view)
{
    view.resizeStarted();
    view.backingScaleChanged();
    view.resizeFinished();

    for (auto* child: view.getSubviews())
        notifyBackingScaleChanged(*child);
}

ViewSurface& requestViewSurface(View& view)
{
    auto& records = androidViewRecords();
    auto found = records.find(&view);

    if (found == records.end())
    {
        auto state = std::make_shared<AndroidViewRecord>();
        state->view = &view;

        auto weak = std::weak_ptr<AndroidViewRecord>(state);

        // Vsync from the choreographer stands in for wl_surface.frame.
        state->record.requestFrameCallback = [weak]
        {
            auto pending = weak.lock();

            if (pending == nullptr || pending->record.surface == nullptr
                || pending->record.frameCallbackPending)
                return;

            auto* choreographer = AChoreographer_getInstance();

            if (choreographer == nullptr)
                return;

            pending->record.frameCallbackPending = true;
            AChoreographer_postFrameCallback64(
                choreographer, androidFrameArrived, new AndroidFrameRequest {weak});
        };

        found = records.emplace(&view, std::move(state)).first;

        // Deferred a turn: the caller has not set its hooks yet.
        Threads::callAsync(
            [weak = std::weak_ptr<AndroidViewRecord>(found->second)]
            {
                if (auto pending = weak.lock())
                    androidSyncViewSurfaces(*pending->view);
            });
    }

    return found->second->record;
}

void androidBindWindowToContentView(View& contentView, AndroidWindowSurface& window)
{
    androidContentViewWindows()[&contentView] = &window;
    androidSyncViewSurfaces(contentView);
}

void androidUnbindWindowFromContentView(View& contentView)
{
    androidReleaseViewSurfaceTree(contentView);
    androidContentViewWindows().erase(&contentView);
}

void androidWindowSurfaceChanged(View& contentView)
{
    androidSyncViewSurfaces(contentView);
}

Image View::renderToImage(float scale)
{
    auto resolvedScale = scale > 0.0f ? scale : linuxDefaultBackingScale;

    return renderNativeContent(resolvedScale);
}

Threads::Async<Image> View::renderToImageAsync(float scale)
{
    auto promise = Threads::AsyncPromise<Image> {};
    auto result = promise.get();

    promise.resolve(renderToImage(scale));

    return result;
}

void View::viewAdded(View& view)
{
    androidSyncViewSurfaces(view);
}

void View::viewRemoved(View& view)
{
    androidReleaseViewSurfaceTree(view);
}

} // namespace eacp::Graphics
