#pragma once

#include "../Core/Element.h"
#include "Box.h"

#include <string>

namespace eacp::React
{
// Every element is a small aggregate: what it *is* in named fields, and how it
// looks in one class string.
//
//     Row {.css = "gap-2 items-center p-2 bg-panel rounded-md"}({
//         Checkbox {.checked = done, .onChange = toggle},
//         Label {.text = title, .css = "flex-1"},
//         Button {.text = "Remove", .onClick = remove},
//     })
//
// Containers are called with their children, which is the only punctuation this
// tier has that JSX does not. Leaves convert to an Element on their own, so
// they can sit straight in a child list.

struct Box
{
    std::string css;
    bool clickable = false;
    Action<> onClick = [] {};
    Action<bool> onHover = [](bool) {};

    Element operator()(Children children) const;
    operator Element() const { return (*this)({}); }
};

// The same box with its direction already set, because a row and a column are
// most of every layout ever written.
struct Row
{
    std::string css;
    bool clickable = false;
    Action<> onClick = [] {};
    Action<bool> onHover = [](bool) {};

    Element operator()(Children children) const;
    operator Element() const { return (*this)({}); }
};

struct Column
{
    std::string css;
    bool clickable = false;
    Action<> onClick = [] {};
    Action<bool> onHover = [](bool) {};

    Element operator()(Children children) const;
    operator Element() const { return (*this)({}); }
};

// Content taller (or wider) than the space it is in, moved by the wheel. Not a
// re-layout: the children keep the rectangles the layout gave them and the box
// moves them, which costs a frame and no paint at all.
struct Scroll
{
    std::string css;

    Element operator()(Children children) const;
    operator Element() const { return (*this)({}); }
};

// A flexible gap. `Spacer {}` pushes what follows it to the far end of the row.
struct Spacer
{
    std::string css;

    operator Element() const;
};

struct Label
{
    std::string text;
    std::string css;

    operator Element() const;
};

struct Button
{
    std::string text;
    Action<> onClick = [] {};

    // A latching button, whose state is the caller's rather than its own: the
    // widget reports the click and `on` says what it draws. Which is what keeps
    // a toggle in a declarative tree from drifting out of step with the state
    // behind it.
    bool toggle = false;
    bool on = false;

    std::string css;

    operator Element() const;
};

struct Checkbox
{
    std::string text;
    bool checked = false;
    Action<bool> onChange = [](bool) {};
    std::string css;

    operator Element() const;
};

struct Field
{
    std::string value;
    std::string placeholder;

    Action<const std::string&> onChange = [](const std::string&) {};
    Action<const std::string&> onSubmit = [](const std::string&) {};
    Action<> onCancel = [] {};

    bool readOnly = false;
    std::string css;

    operator Element() const;
};

struct Slider
{
    float value = 0.f;
    Action<float> onChange = [](float) {};
    bool vertical = false;
    std::string css;

    operator Element() const;
};

struct Knob
{
    float value = 0.f;
    Action<float> onChange = [](float) {};
    std::string css;

    operator Element() const;
};

// The layout pass asks a ScrollBox for this, and nothing else needs it.
const HostType* scrollHostType();

// Mounts a UI::Component you already have.
//
// The escape hatch, and the reason an existing hand-written widget does not
// have to be rewritten to be used from here: it is a leaf of the declarative
// tree, styled and laid out like any other, and what it does inside is its own
// business. `apply` runs on every render, which is where a caller pushes
// whatever this tier knows nothing about.
template <typename WidgetType>
struct Host
{
    Action<WidgetType&> apply = [](WidgetType&) {};

    // What it would like to be where the class string does not say. Zero means
    // the class string had better say.
    Size preferred;

    std::string css;

    operator Element() const;
};

template <typename WidgetType>
struct HostTraits<WidgetType, Host<WidgetType>>
{
    static constexpr const char* name = "host";

    static Style style(const Host<WidgetType>& props) { return styleFor(props.css); }

    static void apply(WidgetType& component,
                      const Host<WidgetType>& props,
                      const Host<WidgetType>*,
                      const Style&)
    {
        props.apply(component);
    }

    static Size
        measure(const WidgetType&, const Host<WidgetType>& props, const Style&)
    {
        return props.preferred;
    }
};

template <typename WidgetType>
Host<WidgetType>::operator Element() const
{
    return makeHost<WidgetType, Host<WidgetType>>(*this);
}
} // namespace eacp::React
