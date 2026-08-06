#include "Layout.h"

#include "../Host/Elements.h"

#include <algorithm>

namespace eacp::React::Layout
{
namespace
{
// The nodes below `node` that take part in its layout: its host descendants,
// stopping at the first one down each branch. Everything in between is a
// function component or a fragment, and neither of those is a box.
void gatherLayoutChildren(Instance& node, Vector<Instance*>& found)
{
    for (auto& child: node.children)
    {
        if (child->isHost())
            found.add(child.get());
        else
            gatherLayoutChildren(*child, found);
    }
}

Vector<Instance*> layoutChildrenOf(Instance& node)
{
    auto found = Vector<Instance*> {};
    gatherLayoutChildren(node, found);

    return found;
}

float clampToStyle(float value,
                   const std::optional<float>& minimum,
                   const std::optional<float>& maximum)
{
    if (minimum.has_value())
        value = std::max(value, *minimum);

    if (maximum.has_value())
        value = std::min(value, *maximum);

    return value;
}

bool hasExplicitCrossSize(const Style& style, Axis axis)
{
    return axis == Axis::Row ? style.height.has_value() : style.width.has_value();
}

Size measureNode(Instance& node)
{
    const auto& style = node.style;
    auto axis = style.direction;

    auto content = Size {};

    if (node.hostType != nullptr && node.hostType->measure != nullptr
        && node.component != nullptr)
        content =
            node.hostType->measure(*node.component, node.props.get(), node.style);

    auto children = layoutChildrenOf(node);

    if (!children.empty())
    {
        auto main = style.gap * static_cast<float>(children.size() - 1);
        auto cross = 0.f;

        for (auto* child: children)
        {
            auto size = measureNode(*child);
            const auto& margin = child->style.margin;

            main += mainOf(size, axis) + mainExtent(margin, axis);
            cross = std::max(cross, crossOf(size, axis) + crossExtent(margin, axis));
        }

        auto fromChildren = sizeFrom(axis, main, cross);

        content.w = std::max(content.w, fromChildren.w);
        content.h = std::max(content.h, fromChildren.h);
    }

    auto measured = Size {content.w + style.padding.horizontal(),
                          content.h + style.padding.vertical()};

    if (style.width.has_value())
        measured.w = *style.width;

    if (style.height.has_value())
        measured.h = *style.height;

    measured.w = clampToStyle(measured.w, style.minWidth, style.maxWidth);
    measured.h = clampToStyle(measured.h, style.minHeight, style.maxHeight);

    node.intrinsic = measured;

    return measured;
}

void arrangeChildren(Instance& node)
{
    auto children = layoutChildrenOf(node);

    if (children.empty())
        return;

    const auto& style = node.style;
    auto axis = style.direction;
    const auto& padding = style.padding;

    auto frameSize = Size {node.frame.w, node.frame.h};
    auto innerMain = mainOf(frameSize, axis) - mainExtent(padding, axis);
    auto innerCross = crossOf(frameSize, axis) - crossExtent(padding, axis);

    auto used = style.gap * static_cast<float>(children.size() - 1);
    auto flexTotal = 0.f;

    for (auto* child: children)
    {
        used +=
            mainOf(child->intrinsic, axis) + mainExtent(child->style.margin, axis);
        flexTotal += child->style.flex;
    }

    // A scroller's content is not squeezed to fit -- that it does not fit is the
    // whole point of one -- so there is never anything to distribute, and a
    // child asking to grow inside it grows to the viewport and no further.
    auto* scroller = node.hostType == scrollHostType()
                         ? static_cast<ScrollView*>(node.component.get())
                         : nullptr;

    auto free = innerMain - used;

    if (scroller != nullptr)
        free = std::max(0.f, free);

    auto offsetMain = mainStart(padding, axis);
    auto spacing = style.gap;

    if (flexTotal <= 0.f && free > 0.f)
    {
        auto count = static_cast<float>(children.size());

        switch (style.justify)
        {
            case Justify::Start:
                break;
            case Justify::Center:
                offsetMain += free * 0.5f;
                break;
            case Justify::End:
                offsetMain += free;
                break;
            case Justify::SpaceBetween:
                if (children.size() > 1)
                    spacing += free / (count - 1.f);
                break;
            case Justify::SpaceAround:
                spacing += free / count;
                offsetMain += free / count * 0.5f;
                break;
        }
    }

    auto scrollOffset = scroller != nullptr ? scroller->getScrollOffset() : 0.f;

    for (auto* child: children)
    {
        const auto& childStyle = child->style;
        const auto& margin = childStyle.margin;

        auto mainSize = mainOf(child->intrinsic, axis);

        if (flexTotal > 0.f && childStyle.flex > 0.f)
            mainSize += free * (childStyle.flex / flexTotal);

        mainSize = std::max(0.f, mainSize);

        auto availableCross = innerCross - crossExtent(margin, axis);
        auto crossSize = crossOf(child->intrinsic, axis);
        auto alignment = childStyle.alignSelf.value_or(style.align);

        if (alignment == Align::Stretch && !hasExplicitCrossSize(childStyle, axis))
            crossSize = std::max(0.f, availableCross);

        auto crossPosition = crossStart(padding, axis) + crossStart(margin, axis);

        if (alignment == Align::Center)
            crossPosition += (availableCross - crossSize) * 0.5f;
        else if (alignment == Align::End)
            crossPosition += availableCross - crossSize;

        offsetMain += mainStart(margin, axis);

        auto position = offsetMain - scrollOffset;

        child->frame = axis == Axis::Row
                           ? Rect {position, crossPosition, mainSize, crossSize}
                           : Rect {crossPosition, position, crossSize, mainSize};

        // setBounds is what runs the widget's own resized() and marks it for a
        // repaint, and it does neither when the rectangle is the one already
        // there -- which is why a re-render that changes nothing costs nothing.
        child->component->setBounds(child->frame);

        arrangeChildren(*child);

        offsetMain += mainSize + mainEnd(margin, axis) + spacing;
    }

    if (scroller != nullptr)
        scroller->setContentLength(offsetMain - spacing + mainEnd(padding, axis));
}
} // namespace

void perform(Instance& node, const Rect& frame)
{
    measureNode(node);

    node.frame = frame;

    if (node.component != nullptr)
        node.component->setBounds(frame);

    arrangeChildren(node);
}
} // namespace eacp::React::Layout
