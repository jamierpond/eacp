#pragma once

#include "../Layout/Style.h"

#include <functional>

namespace eacp::React
{
// The component every declarative container is made of: a rectangle that draws
// its style and holds children the layout pass has placed.
//
// It has no resized() of its own, which is the difference from a JUCE panel.
// Placing children is the layout pass's job, and a component that also did it
// would be two answers to the same question -- the second one silently winning
// whenever the tier moved something.
class BoxView : public UI::Component
{
public:
    BoxView();

    void setStyle(const Style& newStyle);
    const Style& getStyle() const { return style; }

    void paint(UI::Graphics& g) override;

    void mouseEnter(const UI::MouseEvent&) override;
    void mouseExit(const UI::MouseEvent&) override;
    void mouseUp(const UI::MouseEvent& event) override;

    std::function<void()> onClick = [] {};
    std::function<void(bool isOver)> onHover = [](bool) {};

private:
    Style style;
};

// A box whose children run past its edge, moved by the wheel.
//
// The wheel does not re-run layout. The children were placed by it already, in
// this component's own space, so a scroll is the same rectangles moved -- and
// moving a component costs a frame and no paint at all, which is what lets a
// long list scroll for the price of the one component drawing the bar.
class ScrollView final : public BoxView
{
public:
    ScrollView();

    void setAxis(Axis newAxis) { axis = newAxis; }

    // What the layout pass measured the content to be, so this knows how far it
    // may travel. Set on every layout, and it clamps the offset it already has.
    void setContentLength(float length);

    float getScrollOffset() const { return offset; }

    void paintOverChildren(UI::Graphics& g) override;
    bool mouseWheelMove(const UI::MouseEvent& event) override;

private:
    float maximumOffset() const;
    void moveChildrenBy(float delta);

    Axis axis = Axis::Column;
    float offset = 0.f;
    float contentLength = 0.f;
};
} // namespace eacp::React
