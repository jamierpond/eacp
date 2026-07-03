#pragma once

#include "SVGElement.h"

#include <eacp/Graphics/Primitives/Primitives.h>
#include <eacp/Core/Utils/Containers.h>
#include <string>

namespace eacp::SVG
{

struct ColorResult
{
    Graphics::Color color;
    bool isNone = false;
};

ColorResult parseColor(const std::string& value);

// "miter" (the SVG default) unless the value is "round" or "bevel".
Graphics::LineJoin parseLineJoin(const std::string& value);

// "butt" (the SVG default) unless the value is "round" or "square".
Graphics::LineCap parseLineCap(const std::string& value);

// The presentational attributes SVG inherits from ancestor elements —
// notably a root <svg fill="none"> whose stroke-only children carry no
// fill of their own. applied() overlays one element's own attributes.
struct InheritedStyle
{
    InheritedStyle applied(const SVGElement& element) const;

    std::string fill = "black";
    std::string stroke;
    std::string strokeWidth;
    std::string strokeLinejoin;
    std::string strokeLinecap;
};

struct Transform
{
    float translateX = 0.f;
    float translateY = 0.f;
    float scaleX = 1.f;
    float scaleY = 1.f;
    float rotateDeg = 0.f;
};

Transform parseTransform(const std::string& value);

struct TransformOp
{
    enum class Type
    {
        Translate,
        Scale,
        Rotate
    };

    Type type = Type::Translate;
    float a = 0.f;
    float b = 0.f;
};

// The transform functions in source order. Unlike the flattened Transform,
// this preserves composition order: translate(..)scale(..) and
// scale(..)translate(..) place content differently.
Vector<TransformOp> parseTransformList(const std::string& value);

Vector<float> parseNumberList(const std::string& value);

Vector<Graphics::Point> parsePointList(const std::string& value);

} // namespace eacp::SVG
