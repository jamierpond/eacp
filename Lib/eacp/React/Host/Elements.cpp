#include "Elements.h"

#include <algorithm>

namespace eacp::React
{
namespace
{
// The face a run of text is drawn in: the host's, with whatever the class
// string overrode. Needed at measure time as well as paint time, and the
// painter is only in hand for one of those.
Font faceFor(const UI::Component& component, const Style& style)
{
    auto font = component.getHostFont();

    if (style.fontSize.has_value())
        font.pointSize = *style.fontSize;

    if (style.fontStyle.has_value())
        font.style = *style.fontStyle;

    return font;
}

float lineHeightFor(const UI::Component& component, const Font& font)
{
    if (auto* host = component.getHost())
        return host->getLineHeight(font);

    return font.pointSize * 1.3f;
}

void applyTextStyle(UI::Label& label, const Style& style)
{
    label.setColour(style.textColour.value_or(theme().text));
    label.setJustification(style.textAlign.value_or(Justification::Left));

    if (style.fontSize.has_value())
        label.setFontSize(*style.fontSize);

    if (style.fontStyle.has_value())
        label.setFontStyle(*style.fontStyle);
}
} // namespace

template <>
struct HostTraits<BoxView, Box>
{
    static constexpr const char* name = "box";

    static Style style(const Box& props) { return styleFor(props.css); }

    static void
        apply(BoxView& component, const Box& props, const Box*, const Style& style)
    {
        component.setStyle(style);
        component.setInterceptsMouseClicks(props.clickable);
        component.onClick = props.onClick;
        component.onHover = props.onHover;
    }

    static Size measure(const BoxView&, const Box&, const Style&) { return {}; }
};

template <>
struct HostTraits<BoxView, Row>
{
    static constexpr const char* name = "row";

    static Style style(const Row& props)
    {
        auto parsed = styleFor(props.css);
        parsed.direction = Axis::Row;

        return parsed;
    }

    static void
        apply(BoxView& component, const Row& props, const Row*, const Style& style)
    {
        component.setStyle(style);
        component.setInterceptsMouseClicks(props.clickable);
        component.onClick = props.onClick;
        component.onHover = props.onHover;
    }

    static Size measure(const BoxView&, const Row&, const Style&) { return {}; }
};

template <>
struct HostTraits<BoxView, Column>
{
    static constexpr const char* name = "column";

    static Style style(const Column& props)
    {
        auto parsed = styleFor(props.css);
        parsed.direction = Axis::Column;

        return parsed;
    }

    static void apply(BoxView& component,
                      const Column& props,
                      const Column*,
                      const Style& style)
    {
        component.setStyle(style);
        component.setInterceptsMouseClicks(props.clickable);
        component.onClick = props.onClick;
        component.onHover = props.onHover;
    }

    static Size measure(const BoxView&, const Column&, const Style&) { return {}; }
};

template <>
struct HostTraits<ScrollView, Scroll>
{
    static constexpr const char* name = "scroll";

    static Style style(const Scroll& props) { return styleFor(props.css); }

    static void apply(ScrollView& component,
                      const Scroll&,
                      const Scroll*,
                      const Style& style)
    {
        component.setStyle(style);
        component.setAxis(style.direction);
    }

    static Size measure(const ScrollView&, const Scroll&, const Style&)
    {
        return {};
    }
};

template <>
struct HostTraits<UI::Label, Label>
{
    static constexpr const char* name = "label";

    static Style style(const Label& props) { return styleFor(props.css); }

    static void apply(UI::Label& component,
                      const Label& props,
                      const Label* previous,
                      const Style& style)
    {
        if (previous == nullptr || previous->text != props.text)
            component.setText(props.text);

        if (previous == nullptr || previous->css != props.css)
            applyTextStyle(component, style);
    }

    static Size
        measure(const UI::Label& component, const Label& props, const Style& style)
    {
        auto font = faceFor(component, style);

        return {component.measureText(props.text, font),
                lineHeightFor(component, font)};
    }
};

template <>
struct HostTraits<UI::Button, Button>
{
    static constexpr const char* name = "button";

    static Style style(const Button& props) { return styleFor(props.css); }

    static void apply(UI::Button& component,
                      const Button& props,
                      const Button* previous,
                      const Style& style)
    {
        if (previous == nullptr || previous->text != props.text)
            component.setText(props.text);

        if (previous == nullptr || previous->toggle != props.toggle)
            component.setToggleable(props.toggle);

        if (props.toggle && component.getToggleState() != props.on)
            component.setToggleState(props.on);

        if (previous == nullptr || previous->css != props.css)
            component.setAccentColour(style.accent.value_or(theme().accent));

        // Reassigned every render rather than diffed: the whole value of a
        // callback here is that it closes over the state this render saw, so
        // keeping the previous one is exactly the stale-closure bug.
        component.onClick = props.onClick;
    }

    static Size
        measure(const UI::Button& component, const Button& props, const Style& style)
    {
        auto font = faceFor(component, style);

        return {component.measureText(props.text, font) + 28.f,
                lineHeightFor(component, font) + 14.f};
    }
};

template <>
struct HostTraits<UI::Checkbox, Checkbox>
{
    static constexpr const char* name = "checkbox";

    static Style style(const Checkbox& props) { return styleFor(props.css); }

    static void apply(UI::Checkbox& component,
                      const Checkbox& props,
                      const Checkbox* previous,
                      const Style& style)
    {
        if (previous == nullptr || previous->text != props.text)
            component.setText(props.text);

        if (component.isChecked() != props.checked)
            component.setChecked(props.checked);

        if (previous == nullptr || previous->css != props.css)
            component.setAccentColour(style.accent.value_or(theme().accent));

        component.onChange = props.onChange;
    }

    static Size measure(const UI::Checkbox& component,
                        const Checkbox& props,
                        const Style& style)
    {
        auto font = faceFor(component, style);
        auto caption =
            props.text.empty() ? 0.f : component.measureText(props.text, font) + 8.f;

        return {18.f + caption, std::max(20.f, lineHeightFor(component, font))};
    }
};

template <>
struct HostTraits<UI::TextEditor, Field>
{
    static constexpr const char* name = "field";

    static Style style(const Field& props) { return styleFor(props.css); }

    static void apply(UI::TextEditor& component,
                      const Field& props,
                      const Field* previous,
                      const Style& style)
    {
        // Compared against what the widget holds rather than against the
        // previous props, because the widget is where typing lands: a re-render
        // caused by something else entirely must not put the caret back to the
        // end of a field somebody is in the middle of.
        if (component.getText() != props.value)
            component.setText(props.value);

        if (previous == nullptr || previous->placeholder != props.placeholder)
            component.setPlaceholder(props.placeholder);

        if (previous == nullptr || previous->readOnly != props.readOnly)
            component.setReadOnly(props.readOnly);

        if (previous == nullptr || previous->css != props.css)
        {
            component.setColour(style.textColour.value_or(theme().text));
            component.setAccentColour(style.accent.value_or(theme().accent));
        }

        component.onTextChange = props.onChange;
        component.onReturnKey = props.onSubmit;
        component.onEscapeKey = props.onCancel;
    }

    static Size
        measure(const UI::TextEditor& component, const Field&, const Style& style)
    {
        auto font = faceFor(component, style);

        return {160.f, lineHeightFor(component, font) + 12.f};
    }
};

template <>
struct HostTraits<UI::Slider, Slider>
{
    static constexpr const char* name = "slider";

    static Style style(const Slider& props) { return styleFor(props.css); }

    static void apply(UI::Slider& component,
                      const Slider& props,
                      const Slider* previous,
                      const Style& style)
    {
        if (component.getValue() != props.value)
            component.setValue(props.value);

        if (previous == nullptr || previous->css != props.css)
            component.setAccentColour(style.accent.value_or(theme().accent));

        component.onValueChange = props.onChange;
    }

    static Size measure(const UI::Slider&, const Slider& props, const Style&)
    {
        return props.vertical ? Size {24.f, 120.f} : Size {120.f, 24.f};
    }
};

template <>
struct HostTraits<UI::Knob, Knob>
{
    static constexpr const char* name = "knob";

    static Style style(const Knob& props) { return styleFor(props.css); }

    static void apply(UI::Knob& component,
                      const Knob& props,
                      const Knob* previous,
                      const Style& style)
    {
        if (component.getValue() != props.value)
            component.setValue(props.value);

        if (previous == nullptr || previous->css != props.css)
            component.setAccentColour(style.accent.value_or(theme().accent));

        component.onValueChange = props.onChange;
    }

    static Size measure(const UI::Knob&, const Knob&, const Style&)
    {
        return {44.f, 44.f};
    }
};

Element Box::operator()(Children children) const
{
    return makeHost<BoxView, Box>(*this, std::move(children));
}

Element Row::operator()(Children children) const
{
    return makeHost<BoxView, Row>(*this, std::move(children));
}

Element Column::operator()(Children children) const
{
    return makeHost<BoxView, Column>(*this, std::move(children));
}

Element Scroll::operator()(Children children) const
{
    return makeHost<ScrollView, Scroll>(*this, std::move(children));
}

Spacer::operator Element() const
{
    auto stretched = Box {};
    stretched.css = css.empty() ? "flex-1" : "flex-1 " + css;

    return stretched({});
}

Label::operator Element() const
{
    return makeHost<UI::Label, Label>(*this);
}

Button::operator Element() const
{
    return makeHost<UI::Button, Button>(*this);
}

Checkbox::operator Element() const
{
    return makeHost<UI::Checkbox, Checkbox>(*this);
}

Field::operator Element() const
{
    return makeHost<UI::TextEditor, Field>(*this);
}

Slider::operator Element() const
{
    return makeHost<UI::Slider, Slider>(*this);
}

Knob::operator Element() const
{
    return makeHost<UI::Knob, Knob>(*this);
}

const HostType* scrollHostType()
{
    return hostTypeFor<ScrollView, Scroll>();
}
} // namespace eacp::React
