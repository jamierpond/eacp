#include "SVGImage.h"
#include "SVGAttributes.h"
#include "SVGShapes.h"
#include "XMLParser.h"

#include <eacp/Core/Utils/Strings.h>
#include <eacp/Graphics/Image/ImageRender.h>
#include <eacp/Graphics/Primitives/Font.h>

#include <algorithm>
#include <cmath>

namespace eacp::SVG
{

namespace
{
float opacityOf(const SVGElement& element)
{
    auto opacity = element.attr("opacity");
    return opacity.empty() ? 1.f : Strings::parseFloatOr(opacity);
}

void renderShape(Graphics::Context& context,
                 const SVGElement& element,
                 const InheritedStyle& style,
                 const Graphics::Path& path,
                 float sx,
                 float sy)
{
    auto opacity = opacityOf(element);

    auto fill = parseColor(style.fill);
    if (!fill.isNone)
    {
        context.setColor(fill.color.withAlpha(fill.color.a * opacity));
        context.fillPath(path);
    }

    if (style.stroke.empty())
        return;

    auto stroke = parseColor(style.stroke);
    if (stroke.isNone)
        return;

    // Geometry is scaled by (sx, sy), so stroke widths must scale with it;
    // Context has a single line width, so non-uniform stretch averages out.
    auto strokeWidth = style.strokeWidth.empty()
                           ? 1.f
                           : Strings::parseFloatOr(style.strokeWidth, 1.f);
    context.setLineWidth(strokeWidth * (sx + sy) * 0.5f);
    context.setLineJoin(parseLineJoin(style.strokeLinejoin));
    context.setLineCap(parseLineCap(style.strokeLinecap));
    context.setColor(stroke.color.withAlpha(stroke.color.a * opacity));
    context.strokePath(path);
}

void renderText(Graphics::Context& context,
                const SVGElement& element,
                const InheritedStyle& style,
                float sx,
                float sy)
{
    auto text = element.textContent;
    if (text.empty())
        return;

    auto fill = parseColor(style.fill);
    if (fill.isNone)
        return;

    auto x = element.numAttr("x") * sx;
    auto y = element.numAttr("y") * sy;
    auto baseFontSize = element.numAttr("font-size", 16.f);
    auto fontSize = std::min(baseFontSize * sx, baseFontSize * sy);

    auto fontOptions = Graphics::FontOptions()
                           .withName(element.attr("font-family", "Helvetica"))
                           .withSize(fontSize);
    auto font = Graphics::Font(fontOptions);

    auto textWidth = fontSize * static_cast<float>(text.size()) * 0.6f;

    auto anchor = element.attr("text-anchor", "start");
    auto drawX = x;
    if (anchor == "middle")
        drawX = x - textWidth / 2.f;
    else if (anchor == "end")
        drawX = x - textWidth;

    context.setColor(fill.color.withAlpha(fill.color.a * opacityOf(element)));
    context.drawText(text, {drawX, y}, font);
}

constexpr auto degreesToRadians = 0.01745329252f;

// Geometry is pre-scaled by (sx, sy), so translations scale with it while
// scale factors stay unitless. Applied in source order — SVG composes the
// transform list left to right.
void applyTransform(Graphics::Context& context,
                    const std::string& transform,
                    float sx,
                    float sy)
{
    for (auto& op: parseTransformList(transform))
    {
        switch (op.type)
        {
            case TransformOp::Type::Translate:
                context.translate(op.a * sx, op.b * sy);
                break;
            case TransformOp::Type::Scale:
                context.scale(op.a, op.b);
                break;
            case TransformOp::Type::Rotate:
                context.rotate(op.a * degreesToRadians);
                break;
        }
    }
}

void renderElement(Graphics::Context& context,
                   const SVGElement& element,
                   const InheritedStyle& style,
                   float sx,
                   float sy);

void renderGroup(Graphics::Context& context,
                 const SVGElement& element,
                 const InheritedStyle& style,
                 float sx,
                 float sy)
{
    for (auto& child: element.children)
        renderElement(context, child, style, sx, sy);
}

void renderElement(Graphics::Context& context,
                   const SVGElement& element,
                   const InheritedStyle& style,
                   float sx,
                   float sy)
{
    auto resolved = style.applied(element);
    auto transform = element.attr("transform");

    if (!transform.empty())
    {
        context.saveState();
        applyTransform(context, transform, sx, sy);
    }

    if (auto path = makeShapePath(element, sx, sy))
        renderShape(context, element, resolved, *path, sx, sy);
    else if (element.tag == "text")
        renderText(context, element, resolved, sx, sy);
    else if (element.tag == "g")
        renderGroup(context, element, resolved, sx, sy);

    if (!transform.empty())
        context.restoreState();
}

int roundedPixels(float value)
{
    return static_cast<int>(std::lround(value));
}
} // namespace

void render(Graphics::Context& context, const SVGElement& root, float sx, float sy)
{
    auto style = InheritedStyle {}.applied(root);
    for (auto& child: root.children)
        renderElement(context, child, style, sx, sy);
}

Graphics::Image toImage(const std::string& svgMarkup, int width, int height)
{
    auto root = parseXML(svgMarkup);
    if (!root || root->tag != "svg")
        return {};

    auto natural = documentSize(*root);
    if (natural.x <= 0.f || natural.y <= 0.f)
        return {};

    if (width <= 0 && height <= 0)
    {
        width = roundedPixels(natural.x);
        height = roundedPixels(natural.y);
    }
    else if (width <= 0)
    {
        width = roundedPixels(static_cast<float>(height) * natural.x / natural.y);
    }
    else if (height <= 0)
    {
        height = roundedPixels(static_cast<float>(width) * natural.y / natural.x);
    }

    auto sx = static_cast<float>(width) / natural.x;
    auto sy = static_cast<float>(height) / natural.y;

    return Graphics::renderToImage(width,
                                   height,
                                   [&](Graphics::Context& context)
                                   { render(context, *root, sx, sy); });
}

} // namespace eacp::SVG
