#pragma once

#include <eacp/Core/Utils/Common.h>
#include <eacp/Core/Utils/Containers.h>

#include "../Graphics/GraphicsContext.h"
#include "../Layers/Layer.h"
#include "../Graphics/Keyboard.h"

namespace eacp::Graphics
{

enum class MouseEventType
{
    Down,
    Up,
    Dragged,
    Moved,
    Entered,
    Exited,
    Wheel
};

enum class MouseButton
{
    Left = 0,
    Right = 1,
    Middle = 2,
    Other = 3
};

struct MouseEvent
{
    Point pos;
    Point downPos;
    Point delta;
    MouseEventType type = MouseEventType::Down;
    MouseButton button = MouseButton::Left;
    ModifierKeys modifiers;
    int clickCount = 1;
    float pressure = 1.0f;
    double timestamp = 0.0;
};

struct ViewProperties
{
    bool handlesMouseEvents = false;
    bool grabsFocusOnMouseDown = false;
};

class View
{
    using ChildViews = std::initializer_list<std::reference_wrapper<View>>;

public:
    View();
    virtual ~View();

    void repaint();

    // Group opacity for the whole view subtree (chrome, child views and any
    // native GPU/web content), composited over whatever sits behind it. Sibling
    // of Layer::setOpacity, but for an entire View rather than a single layer.
    void setOpacity(float opacity);

    void* getHandle();

    virtual void paint(Context&) {};
    virtual void resized();

    Rect getBounds() const;
    Rect getLocalBounds() const;

    Rect getRelativeBounds(const Rect& ratio) const;

    void setBounds(const Rect& bounds);
    void setBoundsRelative(const Rect& ratio);

    void scaleToFit();
    void scaleToFit(ChildViews views);

    void addChildren(ChildViews views);
    void addSubview(View& view);
    void removeSubview(View& view);
    void removeFromParent();

    void addLayer(Layer& layer);
    void removeLayer(Layer& layer);

    ViewProperties& getProperties() { return properties; }

    View& setHandlesMouseEvents(bool value = true);
    View& setGrabsFocusOnMouseDown(bool value = true);

    Point getMousePosition() const;

    virtual View* hitTest(const Point& point);

    // The single public entry points for input — the exact ones the OS
    // native layer calls. Everything (real events, agent injection, tests)
    // goes through these, so input always passes the same hit-testing /
    // handlesMouseEvents gate. The per-view handlers below are protected so
    // nothing can route around that gate (which would let automation drive
    // a view a human can't).
    void dispatchMouseEvent(const MouseEvent& event);
    void dispatchKeyEvent(const KeyEvent& event);

    bool isHovering() const;

    void focus();
    bool hasFocus() const;

    const Vector<View*>& getSubviews() const { return subviews; }
    const Vector<Layer*>& getLayers() const { return layers; }
    View* getParent() const { return parent; }

    void* getNativeLayer();

protected:
    // Per-view input handlers — override to react. Deliberately NOT public:
    // input must enter through dispatchMouseEvent / dispatchKeyEvent (the
    // same entry the OS uses) so it always passes the view's hit-testing and
    // handlesMouseEvents gate. Making these public again would let an agent
    // or test drive a view a real mouse/keyboard can't reach — the exact
    // bug this guards against.
    virtual void mouseDown(const MouseEvent&) {}
    virtual void mouseUp(const MouseEvent&) {}
    virtual void mouseDragged(const MouseEvent&) {}
    virtual void mouseMoved(const MouseEvent&) {}
    virtual void mouseEntered(const MouseEvent&) {}
    virtual void mouseExited(const MouseEvent&) {}

    // Scroll wheel. event.delta carries the wheel movement (y vertical,
    // x horizontal) in WHEEL_DELTA units.
    virtual void mouseWheel(const MouseEvent&) {}
    virtual void keyDown(const KeyEvent&) {}
    virtual void keyUp(const KeyEvent&) {}

private:
    void handleMouseEvent(const MouseEvent& event);
    Point convertPointToDescendant(const Point& point, View* descendant);
    MouseEvent
        createLocalEvent(const MouseEvent& event, View* target, MouseEventType type);

    void forwardDragOrUpToCapturedTarget(const MouseEvent& event);
    void updateHoverTracking(View* target, const MouseEvent& event);
    void dispatchHoverEvent(View* target, const MouseEvent& event);
    void dispatchExitEvent(const MouseEvent& event);
    void dispatchMouseDown(View* target, const MouseEvent& event);

    void viewAdded(View& view);
    void viewRemoved(View& view);

    Vector<View*> subviews;
    Vector<Layer*> layers;
    View* parent = nullptr;
    View* hoveredView = nullptr;
    View* mouseDownTarget = nullptr;

    ViewProperties properties;

    struct Native;
    Pimpl<Native> impl;
};
} // namespace eacp::Graphics
