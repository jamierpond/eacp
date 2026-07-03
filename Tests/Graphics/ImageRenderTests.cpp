#include <eacp/Graphics/Image/ImageRender.h>

#include <NanoTest/NanoTest.h>

#include <cmath>

using namespace nano;
using eacp::Graphics::Color;
using eacp::Graphics::Context;
using eacp::Graphics::Image;
using eacp::Graphics::Rect;
using eacp::Graphics::renderToImage;

namespace
{
bool roughly(float value, float expected)
{
    return std::abs(value - expected) < 0.02f;
}

bool matches(const Color& color, const Color& expected)
{
    return roughly(color.r, expected.r) && roughly(color.g, expected.g)
           && roughly(color.b, expected.b) && roughly(color.a, expected.a);
}
} // namespace

auto tFillsRect = test("ImageRender/fillsRect") = []
{
    auto image = renderToImage(8,
                               8,
                               [](Context& context)
                               {
                                   context.setColor({1.f, 0.f, 0.f});
                                   context.fillRect({0, 0, 8, 4});
                               });

    check(image.isValid());
    check(matches(image.at(4, 2), {1.f, 0.f, 0.f}));
    check(matches(image.at(4, 6), {0.f, 0.f, 0.f, 0.f}));
};

auto tEmptyDrawStaysTransparent = test("ImageRender/emptyDrawStaysTransparent") = []
{
    auto image = renderToImage(4, 4, [](Context&) {});
    check(image.isValid());
    check(roughly(image.at(2, 2).a, 0.f));
};

auto tInvalidSizeReturnsInvalid = test("ImageRender/invalidSizeReturnsInvalid") = []
{
    check(!renderToImage(0, 8, [](Context&) {}));
    check(!renderToImage(8, -1, [](Context&) {}));
};

auto tDrawImageCopiesPixels = test("ImageRender/drawImageCopiesPixels") = []
{
    auto source = Image(2, 2);
    source.set(0, 0, {1.f, 0.f, 0.f});
    source.set(1, 0, {0.f, 1.f, 0.f});
    source.set(0, 1, {0.f, 0.f, 1.f});
    source.set(1, 1, {1.f, 1.f, 1.f});

    auto image = renderToImage(
        2, 2, [&](Context& context) { context.drawImage(source, {0, 0, 2, 2}); });

    check(matches(image.at(0, 0), {1.f, 0.f, 0.f}));
    check(matches(image.at(1, 0), {0.f, 1.f, 0.f}));
    check(matches(image.at(0, 1), {0.f, 0.f, 1.f}));
    check(matches(image.at(1, 1), {1.f, 1.f, 1.f}));
};

auto tDrawImageScalesIntoRect = test("ImageRender/drawImageScalesIntoRect") = []
{
    auto source = Image(1, 1);
    source.set(0, 0, {1.f, 0.f, 0.f});

    auto image = renderToImage(
        8, 8, [&](Context& context) { context.drawImage(source, {0, 0, 8, 4}); });

    check(matches(image.at(4, 2), {1.f, 0.f, 0.f}));
    check(roughly(image.at(4, 6).a, 0.f));
};

auto tDrawInvalidImageIsANoOp = test("ImageRender/drawInvalidImageIsANoOp") = []
{
    auto image = renderToImage(
        4, 4, [](Context& context) { context.drawImage(Image {}, {0, 0, 4, 4}); });

    check(image.isValid());
    check(roughly(image.at(2, 2).a, 0.f));
};
