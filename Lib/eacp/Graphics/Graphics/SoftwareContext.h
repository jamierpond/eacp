#pragma once

#include "GraphicsContext.h"
#include "../Image/Image.h"

namespace eacp::Graphics
{
// A Context that rasterizes on the CPU into an RGBA8 Image with straight
// (non-premultiplied) alpha, for a platform with no 2D tier of its own to draw
// into and a GPU texture to show the result. Coordinates are in points; the
// image is pointsToPixels pixels per point. Fills are anti-aliased with
// non-zero winding, strokes have butt caps and round joins, and text is drawn
// upright at the transform's scale, whatever its rotation.
class SoftwareContext final : public Context
{
public:
    SoftwareContext(int pixelWidth, int pixelHeight, float pointsToPixelsToUse);

    // Transparent everywhere; the state stack is left alone.
    void clear();

    const Image& getImage() const;
    float getPointsToPixels() const;

    void saveState() override;
    void restoreState() override;

    void translate(float x, float y) override;
    void scale(float x, float y) override;
    void rotate(float angleRadians) override;

    void setColor(const Color& color) override;

    void fillRect(const Rect& rect) override;
    void fillRoundedRect(const Rect& rect, float radius) override;

    void setLineWidth(float width) override;
    void strokeRect(const Rect& rect) override;
    void drawLine(const Point& start, const Point& end) override;

    void fillPath(const Path& p) override;
    void strokePath(const Path& p) override;

    void drawText(const std::string& text,
                  const Point& position,
                  const Font& font) override;

private:
    struct Transform final
    {
        Point apply(const Point& point) const;
        float getScale() const;

        float a = 1.f;
        float b = 0.f;
        float c = 0.f;
        float d = 1.f;
        float tx = 0.f;
        float ty = 0.f;
    };

    struct State final
    {
        Transform transform;
        Color color;
        float lineWidth = 1.f;
    };

    std::uint8_t* getPixels();

    Image image;
    float pointsToPixels = 1.f;
    State state;
    Vector<State> savedStates;
};
} // namespace eacp::Graphics
