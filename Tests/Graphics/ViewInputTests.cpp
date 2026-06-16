#include <eacp/Graphics/Graphics/Keyboard.h>
#include <eacp/Graphics/View/View.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace eacp::Graphics;

// These pin the invariant that keeps "an agent can drive it" identical to
// "a human can drive it": ALL input — real OS events and the debug
// server's input tools alike — enters a View only through
// dispatchMouseEvent / dispatchKeyEvent. The per-view handlers are
// protected, so nothing can route around the hit-testing /
// handlesMouseEvents gate. (Routing around it is the bug that let the
// agent orbit a view a real mouse couldn't.)

namespace
{

struct ProbeView : View
{
    int downs = 0;
    int drags = 0;
    int ups = 0;
    int wheels = 0;
    int keysDown = 0;

    void mouseDown(const MouseEvent&) override { ++downs; }
    void mouseDragged(const MouseEvent&) override { ++drags; }
    void mouseUp(const MouseEvent&) override { ++ups; }
    void mouseWheel(const MouseEvent&) override { ++wheels; }
    void keyDown(const KeyEvent&) override { ++keysDown; }
};

MouseEvent mouseAt(float x, float y, MouseEventType type)
{
    auto event = MouseEvent {};
    event.pos = {x, y};
    event.downPos = {x, y};
    event.type = type;
    return event;
}

} // namespace

// A view that has NOT opted into mouse events is undrivable through the
// canonical dispatch path — the only path real input and the agent's input
// tools take. So automation can never drive a view a human can't.
auto tDispatchRespectsGate = test("View/dispatchRespectsHandlesMouseEvents") = []
{
    auto probe = ProbeView {};
    probe.setBounds({0, 0, 200, 200});

    // Gate closed (the default): the same dispatch the OS and the agent use
    // delivers nothing.
    probe.dispatchMouseEvent(mouseAt(50, 50, MouseEventType::Down));
    probe.dispatchMouseEvent(mouseAt(70, 60, MouseEventType::Dragged));
    probe.dispatchMouseEvent(mouseAt(70, 60, MouseEventType::Up));
    check(probe.downs == 0);
    check(probe.drags == 0);
    check(probe.ups == 0);

    // Gate open: the identical dispatch now drives it.
    probe.setHandlesMouseEvents(true);
    probe.dispatchMouseEvent(mouseAt(50, 50, MouseEventType::Down));
    probe.dispatchMouseEvent(mouseAt(70, 60, MouseEventType::Dragged));
    probe.dispatchMouseEvent(mouseAt(70, 60, MouseEventType::Up));
    check(probe.downs == 1);
    check(probe.drags == 1);
    check(probe.ups == 1);
};

// Keyboard goes through the single dispatchKeyEvent entry — the same one the
// native layer and the agent's `key` tool call.
auto tKeyDispatch = test("View/dispatchKeyEventReachesHandler") = []
{
    auto probe = ProbeView {};

    auto event = KeyEvent {};
    event.type = KeyEventType::Down;
    event.characters = "x";
    probe.dispatchKeyEvent(event);

    check(probe.keysDown == 1);
};
