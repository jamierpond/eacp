#pragma once

#include <eacp/UI/UI.h>

#include <optional>
#include <string>
#include <string_view>

namespace eacp::React
{
using Color = UI::Color;
using Point = UI::Point;
using Rect = UI::Rect;
using Font = UI::Font;
using FontStyle = UI::FontStyle;
using Justification = UI::Justification;

// The palette the class names resolve against: bg-panel, text-dim, border-accent.
const UI::Theme& theme();

// Anything a call site passes as a handler. Action<> is a plain callback,
// Action<bool> is handed the new state, and so on.
template <typename... Args>
using Action = std::function<void(Args...)>;

enum class Axis
{
    Row,
    Column
};

enum class Justify
{
    Start,
    Center,
    End,
    SpaceBetween,
    SpaceAround
};

enum class Align
{
    Start,
    Center,
    End,
    Stretch
};

struct Spacing
{
    float top = 0.f;
    float right = 0.f;
    float bottom = 0.f;
    float left = 0.f;

    constexpr float horizontal() const { return left + right; }
    constexpr float vertical() const { return top + bottom; }
};

struct Size
{
    float w = 0.f;
    float h = 0.f;
};

// What a class string means, once. Nothing writes one of these by hand: every
// element is styled by the string in its `css` field, and this is what that
// parses to.
//
// The optionals are the point. "Unset" has to be distinguishable from "set to
// the default", because an unset text colour means the theme's and an unset
// width means "as wide as you measure" -- neither of which is a number.
struct Style
{
    Axis direction = Axis::Column;

    float gap = 0.f;
    Spacing padding;
    Spacing margin;
    float flex = 0.f;

    Justify justify = Justify::Start;
    Align align = Align::Stretch;
    std::optional<Align> alignSelf;

    std::optional<float> width;
    std::optional<float> height;
    std::optional<float> minWidth;
    std::optional<float> minHeight;
    std::optional<float> maxWidth;
    std::optional<float> maxHeight;

    Color background {0.f, 0.f, 0.f, 0.f};
    float radius = 0.f;
    Color border {0.f, 0.f, 0.f, 0.f};
    float borderWidth = 0.f;

    std::optional<Color> textColour;
    std::optional<Color> accent;
    std::optional<float> fontSize;
    std::optional<FontStyle> fontStyle;
    std::optional<Justification> textAlign;
};

// Turns "flex-1 gap-2 p-4 bg-panel rounded-md" into a Style.
//
// The vocabulary, and the spacing scale, are Tailwind's: a number is four
// points, so p-4 is sixteen and gap-2 is eight. Anything the scale cannot say
// goes in brackets -- w-[137], bg-[#ff8800], text-[15] -- which is the same
// escape hatch and for the same reason.
//
//   layout    flex-row flex-col flex-N grow flex-none
//   spacing   gap-N p-N px-N py-N pt-N pr-N pb-N pl-N m-N mx-N my-N mt-N ...
//   size      w-N h-N w-full h-full w-auto h-auto min-w-N max-w-N min-h-N max-h-N
//   place     justify-start|center|end|between|around
//             items-start|center|end|stretch  self-start|center|end|stretch
//   paint     bg-COLOUR border border-N border-COLOUR
//             rounded rounded-sm|md|lg|xl|full rounded-N
//   text      text-COLOUR text-xs|sm|base|lg|xl|2xl|3xl text-left|center|right
//             font-bold font-normal italic accent-COLOUR
//
// COLOUR is a theme name -- base, panel, text, dim, accent, outline, hover,
// pressed -- or transparent, white, black, or a literal [#rrggbb] / [#rrggbbaa].
// Any of them takes an opacity suffix: bg-panel/50, text-white/70.
Style parseStyle(std::string_view classes);

// The same, memoized on the string. Class strings are overwhelmingly literals,
// so a screenful of elements parses each distinct one once for the life of the
// process and every later render is a hash lookup.
//
// Main thread only, like everything else in this tier.
const Style& styleFor(const std::string& classes);

constexpr float mainOf(const Size& size, Axis axis)
{
    return axis == Axis::Row ? size.w : size.h;
}

constexpr float crossOf(const Size& size, Axis axis)
{
    return axis == Axis::Row ? size.h : size.w;
}

constexpr Size sizeFrom(Axis axis, float main, float cross)
{
    return axis == Axis::Row ? Size {main, cross} : Size {cross, main};
}

constexpr float mainStart(const Spacing& spacing, Axis axis)
{
    return axis == Axis::Row ? spacing.left : spacing.top;
}

constexpr float mainEnd(const Spacing& spacing, Axis axis)
{
    return axis == Axis::Row ? spacing.right : spacing.bottom;
}

constexpr float crossStart(const Spacing& spacing, Axis axis)
{
    return axis == Axis::Row ? spacing.top : spacing.left;
}

constexpr float mainExtent(const Spacing& spacing, Axis axis)
{
    return axis == Axis::Row ? spacing.horizontal() : spacing.vertical();
}

constexpr float crossExtent(const Spacing& spacing, Axis axis)
{
    return axis == Axis::Row ? spacing.vertical() : spacing.horizontal();
}
} // namespace eacp::React
