#pragma once

#include "../Primitives/Path.h"
#include "Layer.h"
#include <eacp/Core/Utils/Common.h>

namespace eacp::Graphics
{
class ShapeLayer : public Layer
{
public:
    ShapeLayer();
    ~ShapeLayer() override;

    void setPath(const Path& path);
    void clearPath();

    void setFillColor(const Color& color);
    void setFillGradient(const LinearGradient& gradient);
    void setStrokeColor(const Color& color);
    void setStrokeWidth(float width);
    void setStrokeJoin(LineJoin join);
    void setStrokeCap(LineCap cap);

    void* getNativeLayer() override;

private:
    struct Native;
    Pimpl<Native> impl;
};

} // namespace eacp::Graphics
