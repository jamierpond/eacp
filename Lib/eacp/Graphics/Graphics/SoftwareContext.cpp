#include "SoftwareContext.h"
#include "../Primitives/Font-Android.h"
#include "../Primitives/Path-Linux.h"

#include <algorithm>
#include <numbers>

namespace eacp::Graphics
{
namespace
{
constexpr auto subsamplesPerRow = 16;
constexpr auto curveStepPixels = 3.f;

using Polygon = Vector<Point>;
using Polygons = Vector<Polygon>;

struct Subpath final
{
    Polygon points;
    bool closed = false;
};

struct Edge final
{
    float x0 = 0.f;
    float y0 = 0.f;
    float x1 = 0.f;
    float y1 = 0.f;
    int direction = 1;
};

struct Crossing final
{
    float x = 0.f;
    int direction = 1;
};

Point lerp(const Point& a, const Point& b, float t)
{
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

int curveSteps(std::initializer_list<Point> controlPolygon)
{
    auto length = 0.f;
    for (auto it = controlPolygon.begin(); it + 1 != controlPolygon.end(); ++it)
        length += it->distanceTo(*(it + 1));

    return std::clamp(int(std::ceil(length / curveStepPixels)), 1, 256);
}

template <typename Mapping>
Vector<Subpath> flatten(const PathGeometry& geometry, Mapping&& map)
{
    auto subpaths = Vector<Subpath>();
    auto start = Point();
    auto current = Point();

    auto open = [&]() -> Polygon&
    {
        if (subpaths.empty() || subpaths.back().closed)
        {
            subpaths.create();
            subpaths.back().points.add(current);
            start = current;
        }
        return subpaths.back().points;
    };

    for (const auto& command: geometry.commands)
    {
        using Verb = PathCommand::Verb;

        switch (command.verb)
        {
            case Verb::move:
                current = map(command.points[0]);
                subpaths.create();
                subpaths.back().points.add(current);
                start = current;
                break;

            case Verb::line:
            {
                auto target = map(command.points[0]);
                if (subpaths.empty())
                    current = target;
                open().add(target);
                current = target;
                break;
            }

            case Verb::quad:
            {
                auto& points = open();
                auto p0 = current;
                auto p1 = map(command.points[0]);
                auto p2 = map(command.points[1]);
                auto steps = curveSteps({p0, p1, p2});

                for (int i = 1; i <= steps; ++i)
                {
                    auto t = float(i) / float(steps);
                    points.add(lerp(lerp(p0, p1, t), lerp(p1, p2, t), t));
                }
                current = p2;
                break;
            }

            case Verb::cubic:
            {
                auto& points = open();
                auto p0 = current;
                auto p1 = map(command.points[0]);
                auto p2 = map(command.points[1]);
                auto p3 = map(command.points[2]);
                auto steps = curveSteps({p0, p1, p2, p3});

                for (int i = 1; i <= steps; ++i)
                {
                    auto t = float(i) / float(steps);
                    auto a = lerp(lerp(p0, p1, t), lerp(p1, p2, t), t);
                    auto b = lerp(lerp(p1, p2, t), lerp(p2, p3, t), t);
                    points.add(lerp(a, b, t));
                }
                current = p3;
                break;
            }

            case Verb::close:
                if (!subpaths.empty())
                    subpaths.back().closed = true;
                current = start;
                break;
        }
    }

    return subpaths;
}

float signedArea(const Polygon& polygon)
{
    auto area = 0.f;
    for (int i = 0; i < polygon.size(); ++i)
    {
        const auto& a = polygon[i];
        const auto& b = polygon[(i + 1) % polygon.size()];
        area += a.x * b.y - b.x * a.y;
    }
    return area * 0.5f;
}

// Stroke pieces overlap, so each is wound the same way round for non-zero
// winding to add them up rather than cancel them out.
void addWoundPositively(Polygons& polygons, Polygon polygon)
{
    if (signedArea(polygon) < 0.f)
        std::reverse(polygon.begin(), polygon.end());
    polygons.add(std::move(polygon));
}

void addDisc(Polygons& polygons, const Point& centre, float radius)
{
    auto steps = std::clamp(
        int(std::ceil(2.f * std::numbers::pi_v<float> * radius / 1.5f)), 8, 96);
    auto disc = Polygon();

    for (int i = 0; i < steps; ++i)
    {
        auto angle = 2.f * std::numbers::pi_v<float> * float(i) / float(steps);
        disc.add({centre.x + radius * std::cos(angle),
                  centre.y + radius * std::sin(angle)});
    }

    polygons.add(std::move(disc));
}

void addSegment(Polygons& polygons, const Point& a, const Point& b, float half)
{
    auto along = (b - a).normalized();
    auto normal = Point(-along.y * half, along.x * half);

    addWoundPositively(polygons, {a + normal, b + normal, b - normal, a - normal});
}

Polygons strokePolygons(const Vector<Subpath>& subpaths, float width)
{
    auto polygons = Polygons();
    auto half = width * 0.5f;

    for (const auto& subpath: subpaths)
    {
        auto points = Polygon();
        for (const auto& point: subpath.points)
            if (points.empty() || points.back().distanceTo(point) > 1e-3f)
                points.add(point);

        if (subpath.closed && points.size() > 2
            && points.back().distanceTo(points.front()) <= 1e-3f)
            points.pop_back();

        if (points.size() < 2)
            continue;

        auto count = points.size();
        auto segments = subpath.closed ? count : count - 1;

        for (int i = 0; i < segments; ++i)
            addSegment(polygons, points[i], points[(i + 1) % count], half);

        for (int i = subpath.closed ? 0 : 1;
             i < (subpath.closed ? count : count - 1);
             ++i)
            addDisc(polygons, points[i], half);
    }

    return polygons;
}

void blend(std::uint8_t* pixel, const Color& color, float coverage)
{
    auto source = std::clamp(color.a, 0.f, 1.f) * std::min(coverage, 1.f);
    if (source <= 0.f)
        return;

    auto dest = float(pixel[3]) / 255.f;
    auto out = source + dest * (1.f - source);
    auto destWeight = dest * (1.f - source);

    auto channel = [&](int index, float value)
    {
        auto mixed = (std::clamp(value, 0.f, 1.f) * source
                      + float(pixel[index]) / 255.f * destWeight)
                     / out;
        pixel[index] = static_cast<std::uint8_t>(std::lround(mixed * 255.f));
    };

    channel(0, color.r);
    channel(1, color.g);
    channel(2, color.b);
    pixel[3] = static_cast<std::uint8_t>(std::lround(out * 255.f));
}

// Adds weight across [from, to) of a row of cells, as the differences that a
// running sum turns back into per-cell coverage, fractional at both ends.
void addSpan(Vector<float>& deltas, float from, float to, float weight)
{
    auto first = int(std::floor(from));
    auto last = int(std::floor(to));

    if (first == last)
    {
        deltas[first] += weight * (to - from);
        deltas[first + 1] -= weight * (to - from);
        return;
    }

    deltas[first] += weight * (float(first + 1) - from);
    deltas[first + 1] += weight * (from - float(first));
    deltas[last] -= weight * (1.f - (to - float(last)));
    deltas[last + 1] -= weight * (to - float(last));
}

void fillPolygons(const Polygons& polygons,
                  const Color& color,
                  std::uint8_t* pixels,
                  int width,
                  int height)
{
    auto edges = Vector<Edge>();
    auto minX = std::numeric_limits<float>::max();
    auto minY = minX;
    auto maxX = std::numeric_limits<float>::lowest();
    auto maxY = maxX;

    for (const auto& polygon: polygons)
        for (int i = 0; i < polygon.size(); ++i)
        {
            auto a = polygon[i];
            auto b = polygon[(i + 1) % polygon.size()];

            minX = std::min(minX, a.x);
            maxX = std::max(maxX, a.x);
            minY = std::min(minY, a.y);
            maxY = std::max(maxY, a.y);

            if (a.y == b.y)
                continue;

            if (a.y < b.y)
                edges.add({a.x, a.y, b.x, b.y, 1});
            else
                edges.add({b.x, b.y, a.x, a.y, -1});
        }

    auto left = std::max(0, int(std::floor(minX)));
    auto right = std::min(width, int(std::ceil(maxX)) + 1);
    auto top = std::max(0, int(std::floor(minY)));
    auto bottom = std::min(height, int(std::ceil(maxY)) + 1);

    if (edges.empty() || left >= right || top >= bottom)
        return;

    std::sort(edges.begin(),
              edges.end(),
              [](const Edge& a, const Edge& b) { return a.y0 < b.y0; });

    auto span = right - left;
    auto deltas = Vector<float>(span + 2);
    auto active = Vector<Edge>();
    auto crossings = Vector<Crossing>();
    auto next = 0;
    auto weight = 1.f / float(subsamplesPerRow);

    for (int row = top; row < bottom; ++row)
    {
        while (next < edges.size() && edges[next].y0 < float(row + 1))
            active.add(edges[next++]);

        active.removeIndexesMatching([&](const Edge& edge)
                                     { return edge.y1 <= float(row); });

        if (active.empty())
            continue;

        std::fill(deltas.begin(), deltas.end(), 0.f);

        for (int sample = 0; sample < subsamplesPerRow; ++sample)
        {
            auto y = float(row) + (float(sample) + 0.5f) * weight;
            crossings.clear();

            for (const auto& edge: active)
                if (edge.y0 <= y && y < edge.y1)
                    crossings.add({edge.x0
                                       + (y - edge.y0) * (edge.x1 - edge.x0)
                                             / (edge.y1 - edge.y0),
                                   edge.direction});

            std::sort(crossings.begin(),
                      crossings.end(),
                      [](const Crossing& a, const Crossing& b)
                      { return a.x < b.x; });

            auto winding = 0;
            for (int i = 0; i + 1 < crossings.size(); ++i)
            {
                winding += crossings[i].direction;
                if (winding == 0)
                    continue;

                auto from =
                    std::clamp(crossings[i].x - float(left), 0.f, float(span));
                auto to =
                    std::clamp(crossings[i + 1].x - float(left), 0.f, float(span));
                if (to > from)
                    addSpan(deltas, from, to, weight);
            }
        }

        auto coverage = 0.f;
        auto* line = pixels + (row * width + left) * 4;
        for (int x = 0; x < span; ++x)
        {
            coverage += deltas[x];
            if (coverage > 1.f / 512.f)
                blend(line + x * 4, color, coverage);
        }
    }
}
} // namespace

Point SoftwareContext::Transform::apply(const Point& point) const
{
    return {a * point.x + c * point.y + tx, b * point.x + d * point.y + ty};
}

float SoftwareContext::Transform::getScale() const
{
    return std::sqrt(std::abs(a * d - b * c));
}

SoftwareContext::SoftwareContext(int pixelWidth,
                                 int pixelHeight,
                                 float pointsToPixelsToUse)
    : image(pixelWidth, pixelHeight)
    , pointsToPixels(pointsToPixelsToUse)
{
    state.transform.a = pointsToPixels;
    state.transform.d = pointsToPixels;
}

void SoftwareContext::clear()
{
    if (auto* pixels = getPixels())
        std::fill(pixels, pixels + image.width() * image.height() * 4, 0);
}

const Image& SoftwareContext::getImage() const
{
    return image;
}

float SoftwareContext::getPointsToPixels() const
{
    return pointsToPixels;
}

std::uint8_t* SoftwareContext::getPixels()
{
    return image.prepareForOverwrite(image.width(), image.height());
}

void SoftwareContext::saveState()
{
    savedStates.add(state);
}

void SoftwareContext::restoreState()
{
    if (savedStates.empty())
        return;

    state = savedStates.back();
    savedStates.pop_back();
}

void SoftwareContext::translate(float x, float y)
{
    auto& t = state.transform;
    t.tx += t.a * x + t.c * y;
    t.ty += t.b * x + t.d * y;
}

void SoftwareContext::scale(float x, float y)
{
    auto& t = state.transform;
    t.a *= x;
    t.b *= x;
    t.c *= y;
    t.d *= y;
}

void SoftwareContext::rotate(float angleRadians)
{
    auto& t = state.transform;
    auto cosine = std::cos(angleRadians);
    auto sine = std::sin(angleRadians);
    auto old = t;

    t.a = old.a * cosine + old.c * sine;
    t.b = old.b * cosine + old.d * sine;
    t.c = old.c * cosine - old.a * sine;
    t.d = old.d * cosine - old.b * sine;
}

void SoftwareContext::setColor(const Color& color)
{
    state.color = color;
}

void SoftwareContext::fillRect(const Rect& rect)
{
    auto path = Path();
    path.addRect(rect);
    fillPath(path);
}

void SoftwareContext::fillRoundedRect(const Rect& rect, float radius)
{
    auto path = Path();
    path.addRoundedRect(rect, clampedCornerRadius(rect, radius));
    fillPath(path);
}

void SoftwareContext::setLineWidth(float width)
{
    state.lineWidth = width;
}

void SoftwareContext::strokeRect(const Rect& rect)
{
    auto path = Path();
    path.addRect(rect);
    strokePath(path);
}

void SoftwareContext::drawLine(const Point& start, const Point& end)
{
    auto path = Path();
    path.moveTo(start);
    path.lineTo(end);
    strokePath(path);
}

void SoftwareContext::fillPath(const Path& p)
{
    auto polygons = Polygons();
    for (auto& subpath:
         flatten(getPathGeometry(p),
                 [&](const Point& q) { return state.transform.apply(q); }))
        if (subpath.points.size() > 2)
            polygons.add(std::move(subpath.points));

    if (auto* pixels = getPixels())
        fillPolygons(polygons, state.color, pixels, image.width(), image.height());
}

void SoftwareContext::strokePath(const Path& p)
{
    auto subpaths =
        flatten(getPathGeometry(p),
                [&](const Point& q) { return state.transform.apply(q); });
    auto width = state.lineWidth * state.transform.getScale();

    if (auto* pixels = getPixels())
        fillPolygons(strokePolygons(subpaths, width),
                     state.color,
                     pixels,
                     image.width(),
                     image.height());
}

void SoftwareContext::drawText(const std::string& text,
                               const Point& position,
                               const Font& font)
{
    const auto& resolved = AndroidFonts::resolve(font);
    auto* pixels = getPixels();
    if (!resolved.face || !pixels)
        return;

    auto origin = state.transform.apply(position);
    auto pixelsPerPoint = state.transform.getScale();
    auto pixelSize = resolved.size * pixelsPerPoint;
    auto run = AndroidFonts::layout(resolved, text);
    auto baseline = int(std::lround(origin.y));

    for (int i = 0; i < run.codepoints.size(); ++i)
    {
        const auto& glyph =
            AndroidFonts::getGlyph(resolved, run.codepoints[i], pixelSize);
        auto left = int(std::lround(origin.x + run.penPositions[i] * pixelsPerPoint))
                    + glyph.left;
        auto top = baseline + glyph.top;

        for (int y = std::max(0, -top); y < glyph.height; ++y)
        {
            if (top + y >= image.height())
                break;

            for (int x = std::max(0, -left); x < glyph.width; ++x)
            {
                if (left + x >= image.width())
                    break;

                if (auto value = glyph.coverage[y * glyph.width + x])
                    blend(pixels + ((top + y) * image.width() + left + x) * 4,
                          state.color,
                          float(value) / 255.f);
            }
        }
    }
}
} // namespace eacp::Graphics
