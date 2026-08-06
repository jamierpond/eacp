#include "Style.h"

#include <cstdlib>
#include <unordered_map>

namespace eacp::React
{
const UI::Theme& theme()
{
    return UI::defaultTheme();
}

namespace
{
// Tailwind's scale: one unit is four points, so p-4 is sixteen and gap-2 is
// eight. Keeping the same numbers means the muscle memory carries over, which
// is most of the value of using the vocabulary at all.
constexpr auto spacingUnit = 4.f;

bool startsWith(std::string_view token, std::string_view prefix)
{
    return token.size() >= prefix.size() && token.substr(0, prefix.size()) == prefix;
}

// The value after a prefix: "p-4" past "p-" is "4", and "w-[137]" past "w-" is
// "[137]" -- brackets left on, because whether they were there is what decides
// between a scale step and a literal.
std::string_view valueOf(std::string_view token, std::string_view prefix)
{
    return token.substr(prefix.size());
}

bool isBracketed(std::string_view text)
{
    return text.size() >= 2 && text.front() == '[' && text.back() == ']';
}

std::string_view unbracket(std::string_view text)
{
    return isBracketed(text) ? text.substr(1, text.size() - 2) : text;
}

std::optional<float> toNumber(std::string_view text)
{
    text = unbracket(text);

    if (text.empty())
        return {};

    // A copy, because strtof needs a terminator and a class string is parsed
    // once per distinct string for the life of the process.
    auto digits = std::string {text};
    auto* end = static_cast<char*>(nullptr);
    auto value = std::strtof(digits.c_str(), &end);

    if (end != digits.c_str() + digits.size())
        return {};

    return value;
}

// A step on the spacing scale, or -- in brackets -- the number itself. Which is
// the whole point of the brackets: the scale is for the ninety per cent of
// values that should agree with everything around them, and a literal is for
// the one that has to be 137.
std::optional<float> toLength(std::string_view text)
{
    if (isBracketed(text))
        return toNumber(text);

    if (auto number = toNumber(text))
        return *number * spacingUnit;

    return {};
}

int hexDigit(char character)
{
    if (character >= '0' && character <= '9')
        return character - '0';

    if (character >= 'a' && character <= 'f')
        return character - 'a' + 10;

    if (character >= 'A' && character <= 'F')
        return character - 'A' + 10;

    return -1;
}

std::optional<Color> hexColour(std::string_view text)
{
    if (text.empty() || text.front() != '#')
        return {};

    auto digits = text.substr(1);

    if (digits.size() != 6 && digits.size() != 8)
        return {};

    auto channel = [&digits](int index)
    {
        auto high = hexDigit(digits[static_cast<std::size_t>(index) * 2]);
        auto low = hexDigit(digits[static_cast<std::size_t>(index) * 2 + 1]);

        if (high < 0 || low < 0)
            return -1.f;

        return static_cast<float>(high * 16 + low) / 255.f;
    };

    auto red = channel(0);
    auto green = channel(1);
    auto blue = channel(2);
    auto alpha = digits.size() == 8 ? channel(3) : 1.f;

    if (red < 0.f || green < 0.f || blue < 0.f || alpha < 0.f)
        return {};

    return Color {red, green, blue, alpha};
}

// A colour name with an optional opacity suffix: panel, white/70, [#ff8800]/50.
std::optional<Color> toColour(std::string_view text)
{
    auto alpha = 1.f;
    auto slash = text.rfind('/');

    if (slash != std::string_view::npos)
    {
        auto percentage = toNumber(text.substr(slash + 1));

        if (!percentage.has_value())
            return {};

        alpha = *percentage / 100.f;
        text = text.substr(0, slash);
    }

    if (text.size() >= 2 && text.front() == '[' && text.back() == ']')
        text = text.substr(1, text.size() - 2);

    auto named = [&]() -> std::optional<Color>
    {
        const auto& palette = theme();

        if (text == "base")
            return palette.background;
        if (text == "panel")
            return palette.panel;
        if (text == "text")
            return palette.text;
        if (text == "dim")
            return palette.dimText;
        if (text == "accent")
            return palette.accent;
        if (text == "accent-dim")
            return palette.accentDim;
        if (text == "outline")
            return palette.outline;
        if (text == "hover")
            return palette.hover;
        if (text == "pressed")
            return palette.pressed;
        if (text == "white")
            return Color::white();
        if (text == "black")
            return Color::black();
        if (text == "transparent")
            return Color {0.f, 0.f, 0.f, 0.f};

        return hexColour(text);
    }();

    if (!named.has_value())
        return {};

    return named->withAlpha(named->a * alpha);
}

std::optional<float> namedFontSize(std::string_view text)
{
    if (text == "xs")
        return 11.f;
    if (text == "sm")
        return 12.f;
    if (text == "base")
        return 13.f;
    if (text == "lg")
        return 16.f;
    if (text == "xl")
        return 20.f;
    if (text == "2xl")
        return 24.f;
    if (text == "3xl")
        return 30.f;

    return {};
}

std::optional<float> namedRadius(std::string_view text)
{
    if (text.empty())
        return 5.f;
    if (text == "none")
        return 0.f;
    if (text == "sm")
        return 3.f;
    if (text == "md")
        return 6.f;
    if (text == "lg")
        return 8.f;
    if (text == "xl")
        return 12.f;
    if (text == "full")
        return 9999.f;

    return {};
}

void applyEdge(Spacing& spacing, std::string_view sides, float amount)
{
    if (sides.empty())
        spacing = {amount, amount, amount, amount};
    else if (sides == "x")
        spacing.left = spacing.right = amount;
    else if (sides == "y")
        spacing.top = spacing.bottom = amount;
    else if (sides == "t")
        spacing.top = amount;
    else if (sides == "r")
        spacing.right = amount;
    else if (sides == "b")
        spacing.bottom = amount;
    else if (sides == "l")
        spacing.left = amount;
}

// The spacing family shares one shape -- a letter, an optional side, a number --
// so p/px/pt and m/mx/mt are one rule read twice rather than fourteen.
bool applySpacingToken(std::string_view token,
                       std::string_view letter,
                       Spacing& spacing)
{
    if (!startsWith(token, letter))
        return false;

    auto rest = token.substr(letter.size());
    auto dash = rest.find('-');

    if (dash == std::string_view::npos)
        return false;

    auto sides = rest.substr(0, dash);

    if (sides.size() > 1)
        return false;

    if (!sides.empty() && sides != "x" && sides != "y" && sides != "t"
        && sides != "r" && sides != "b" && sides != "l")
        return false;

    auto amount = toLength(rest.substr(dash + 1));

    if (!amount.has_value())
        return false;

    applyEdge(spacing, sides, *amount);

    return true;
}

std::optional<Align> toAlign(std::string_view text)
{
    if (text == "start")
        return Align::Start;
    if (text == "center")
        return Align::Center;
    if (text == "end")
        return Align::End;
    if (text == "stretch")
        return Align::Stretch;

    return {};
}

void applyToken(Style& style, std::string_view token)
{
    if (token.empty())
        return;

    if (token == "flex-row")
    {
        style.direction = Axis::Row;
        return;
    }

    if (token == "flex-col")
    {
        style.direction = Axis::Column;
        return;
    }

    if (token == "grow")
    {
        style.flex = 1.f;
        return;
    }

    if (token == "flex-none")
    {
        style.flex = 0.f;
        return;
    }

    if (startsWith(token, "flex-"))
    {
        if (auto value = toNumber(valueOf(token, "flex-")))
            style.flex = *value;

        return;
    }

    if (startsWith(token, "gap-"))
    {
        if (auto value = toLength(valueOf(token, "gap-")))
            style.gap = *value;

        return;
    }

    if (applySpacingToken(token, "p", style.padding))
        return;

    if (applySpacingToken(token, "m", style.margin))
        return;

    if (startsWith(token, "min-w-"))
    {
        style.minWidth = toLength(valueOf(token, "min-w-"));
        return;
    }

    if (startsWith(token, "min-h-"))
    {
        style.minHeight = toLength(valueOf(token, "min-h-"));
        return;
    }

    if (startsWith(token, "max-w-"))
    {
        style.maxWidth = toLength(valueOf(token, "max-w-"));
        return;
    }

    if (startsWith(token, "max-h-"))
    {
        style.maxHeight = toLength(valueOf(token, "max-h-"));
        return;
    }

    if (startsWith(token, "w-"))
    {
        auto value = valueOf(token, "w-");

        // full is the parent's, which is what stretching across already does --
        // saying it explicitly is for a child whose row centres everything else.
        if (value == "full")
            style.alignSelf = Align::Stretch;
        else if (value != "auto")
            style.width = toLength(value);

        return;
    }

    if (startsWith(token, "h-"))
    {
        auto value = valueOf(token, "h-");

        if (value == "full")
            style.alignSelf = Align::Stretch;
        else if (value != "auto")
            style.height = toLength(value);

        return;
    }

    if (startsWith(token, "justify-"))
    {
        auto value = valueOf(token, "justify-");

        if (value == "start")
            style.justify = Justify::Start;
        else if (value == "center")
            style.justify = Justify::Center;
        else if (value == "end")
            style.justify = Justify::End;
        else if (value == "between")
            style.justify = Justify::SpaceBetween;
        else if (value == "around")
            style.justify = Justify::SpaceAround;

        return;
    }

    if (startsWith(token, "items-"))
    {
        if (auto value = toAlign(valueOf(token, "items-")))
            style.align = *value;

        return;
    }

    if (startsWith(token, "self-"))
    {
        style.alignSelf = toAlign(valueOf(token, "self-"));
        return;
    }

    if (startsWith(token, "bg-"))
    {
        if (auto colour = toColour(valueOf(token, "bg-")))
            style.background = *colour;

        return;
    }

    if (startsWith(token, "accent-"))
    {
        style.accent = toColour(valueOf(token, "accent-"));
        return;
    }

    if (token == "border")
    {
        style.borderWidth = 1.f;

        if (style.border.a == 0.f)
            style.border = theme().outline;

        return;
    }

    if (startsWith(token, "border-"))
    {
        auto value = valueOf(token, "border-");

        // border-2 is a width and border-accent is a colour, told apart by
        // whether the value is a number -- which is how Tailwind reads them too.
        if (auto width = toNumber(value))
        {
            style.borderWidth = *width;

            if (style.border.a == 0.f)
                style.border = theme().outline;
        }
        else if (auto colour = toColour(value))
        {
            style.border = *colour;

            if (style.borderWidth == 0.f)
                style.borderWidth = 1.f;
        }

        return;
    }

    if (token == "rounded" || startsWith(token, "rounded-"))
    {
        auto value =
            token == "rounded" ? std::string_view {} : valueOf(token, "rounded-");

        if (auto named = namedRadius(value))
            style.radius = *named;
        else if (auto number = toNumber(value))
            style.radius = *number;

        return;
    }

    if (token == "font-bold")
    {
        style.fontStyle = FontStyle::Bold;
        return;
    }

    if (token == "font-normal")
    {
        style.fontStyle = FontStyle::Regular;
        return;
    }

    if (token == "italic")
    {
        style.fontStyle = style.fontStyle == FontStyle::Bold ? FontStyle::BoldItalic
                                                             : FontStyle::Italic;
        return;
    }

    if (startsWith(token, "text-"))
    {
        auto value = valueOf(token, "text-");

        if (value == "left")
            style.textAlign = Justification::Left;
        else if (value == "center")
            style.textAlign = Justification::Centred;
        else if (value == "right")
            style.textAlign = Justification::Right;
        else if (auto size = namedFontSize(value))
            style.fontSize = *size;
        else if (auto colour = toColour(value))
            style.textColour = *colour;
        else if (auto number = toNumber(value))
            style.fontSize = *number;
    }
}
} // namespace

Style parseStyle(std::string_view classes)
{
    auto style = Style {};
    auto position = std::size_t {0};

    while (position < classes.size())
    {
        auto start = classes.find_first_not_of(" \t\n", position);

        if (start == std::string_view::npos)
            break;

        auto end = classes.find_first_of(" \t\n", start);

        if (end == std::string_view::npos)
            end = classes.size();

        applyToken(style, classes.substr(start, end - start));
        position = end;
    }

    return style;
}

const Style& styleFor(const std::string& classes)
{
    static auto parsed = std::unordered_map<std::string, Style> {};

    auto found = parsed.find(classes);

    if (found != parsed.end())
        return found->second;

    return parsed.emplace(classes, parseStyle(classes)).first->second;
}
} // namespace eacp::React
