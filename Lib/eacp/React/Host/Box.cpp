#include "Box.h"

#include <algorithm>

namespace eacp::React
{
namespace
{
bool sameColour(const Color& a, const Color& b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

bool drawsTheSame(const Style& a, const Style& b)
{
    return sameColour(a.background, b.background) && sameColour(a.border, b.border)
           && a.radius == b.radius && a.borderWidth == b.borderWidth;
}
} // namespace

BoxView::BoxView() = default;

void BoxView::setStyle(const Style& newStyle)
{
    auto changed = !drawsTheSame(style, newStyle);
    style = newStyle;

    if (changed)
        repaint();
}

void BoxView::paint(UI::Graphics& g)
{
    auto bounds = getLocalBounds();

    if (style.background.a > 0.f)
    {
        g.setColour(style.background);

        if (style.radius > 0.f)
            g.fillRoundedRect(bounds, style.radius);
        else
            g.fillRect(bounds);
    }

    if (style.borderWidth > 0.f && style.border.a > 0.f)
    {
        g.setColour(style.border);

        if (style.radius > 0.f)
            g.drawRoundedRect(bounds, style.radius, style.borderWidth);
        else
            g.drawRect(bounds, style.borderWidth);
    }
}

void BoxView::mouseEnter(const UI::MouseEvent&)
{
    onHover(true);
}

void BoxView::mouseExit(const UI::MouseEvent&)
{
    onHover(false);
}

void BoxView::mouseUp(const UI::MouseEvent& event)
{
    if (getLocalBounds().contains(event.position))
        onClick();
}

ScrollView::ScrollView()
{
    setInterceptsMouseClicks(true);
}

float ScrollView::maximumOffset() const
{
    auto viewport = axis == Axis::Row ? getWidth() : getHeight();

    return std::max(0.f, contentLength - viewport);
}

void ScrollView::setContentLength(float length)
{
    contentLength = length;

    auto clamped = std::clamp(offset, 0.f, maximumOffset());

    if (clamped != offset)
        moveChildrenBy(clamped - offset);
}

void ScrollView::moveChildrenBy(float delta)
{
    offset += delta;

    for (auto* child: getChildren())
    {
        auto bounds = child->getBounds();

        if (axis == Axis::Row)
            bounds.x -= delta;
        else
            bounds.y -= delta;

        child->setBounds(bounds);
    }

    // The children moved rather than changed, so none of them repaints. What
    // does is this component, whose position indicator is drawn over them.
    repaint();
}

bool ScrollView::mouseWheelMove(const UI::MouseEvent& event)
{
    if (maximumOffset() <= 0.f)
        return false;

    // A trackpad reports points and is applied as it comes; a notched wheel
    // reports lines, and only this component knows what a line is worth here.
    auto raw = axis == Axis::Row ? event.wheelDelta.x : event.wheelDelta.y;
    auto step = event.preciseWheel ? raw : raw * 40.f;

    auto target = std::clamp(offset - step, 0.f, maximumOffset());

    if (target != offset)
        moveChildrenBy(target - offset);

    return true;
}

void ScrollView::paintOverChildren(UI::Graphics& g)
{
    auto maximum = maximumOffset();

    if (maximum <= 0.f)
        return;

    const auto& palette = theme();
    auto bounds = getLocalBounds();
    auto thickness = 4.f;

    if (axis == Axis::Row)
    {
        auto track = bounds.fromBottom(thickness);
        auto proportion = bounds.w / contentLength;
        auto thumbWidth = std::max(24.f, bounds.w * proportion);
        auto thumbX = (bounds.w - thumbWidth) * (offset / maximum);

        g.setColour(palette.outline);
        g.fillRoundedRect(track, thickness * 0.5f);

        g.setColour(palette.dimText);
        g.fillRoundedRect({thumbX, track.y, thumbWidth, thickness},
                          thickness * 0.5f);

        return;
    }

    auto track = bounds.fromRight(thickness);
    auto proportion = bounds.h / contentLength;
    auto thumbHeight = std::max(24.f, bounds.h * proportion);
    auto thumbY = (bounds.h - thumbHeight) * (offset / maximum);

    g.setColour(palette.outline);
    g.fillRoundedRect(track, thickness * 0.5f);

    g.setColour(palette.dimText);
    g.fillRoundedRect({track.x, thumbY, thickness, thumbHeight}, thickness * 0.5f);
}
} // namespace eacp::React
