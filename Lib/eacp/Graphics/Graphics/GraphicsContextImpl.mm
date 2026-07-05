#include "GraphicsContextImpl.h"
#import <CoreText/CoreText.h>

namespace eacp::Graphics
{

static CFRef<CGColorSpaceRef> getColorSpace()
{
    return {CGColorSpaceCreateDeviceRGB()};
}

static CFRef<CGColorRef> getColorRef(const Color& c)
{
    auto colorSpace = getColorSpace();

    CGFloat components[4] = {c.r, c.g, c.b, c.a};
    return CGColorCreate(colorSpace, components);
}

void MacOSContext::drawText(const std::string& text,
                            const Point& position,
                            const Font& font)
{
    if (text.empty())
        return;

    auto ctFont = (CTFontRef) font.getHandle();

    if (!ctFont)
        return;

    auto cfString = CFRef<CFStringRef>(
        CFStringCreateWithCString(nullptr, text.c_str(), kCFStringEncodingUTF8));

    auto attrString = CFRef<CFMutableAttributedStringRef>(
        CFAttributedStringCreateMutable(nullptr, 0));

    CFAttributedStringReplaceString(attrString, CFRangeMake(0, 0), cfString);
    CFAttributedStringSetAttribute(attrString,
                                   CFRangeMake(0, CFStringGetLength(cfString)),
                                   kCTFontAttributeName,
                                   ctFont);

    auto textColor = getColorRef(currentColor);

    CFAttributedStringSetAttribute(attrString,
                                   CFRangeMake(0, CFStringGetLength(cfString)),
                                   kCTForegroundColorAttributeName,
                                   textColor);


    auto line = CFRef<CTLineRef>(CTLineCreateWithAttributedString(attrString));

    CGContextSetTextMatrix(context, CGAffineTransformIdentity);

    CGContextTranslateCTM(context, position.x, position.y);
    CGContextScaleCTM(context, 1.0, -1.0);

    CTLineDraw(line, context);

    CGContextScaleCTM(context, 1.0, -1.0);
    CGContextTranslateCTM(context, -position.x, -position.y);
}

// CGContextDrawImage assumes a bottom-left origin; the context draws
// with the top-left origin View::paint uses, so flip around the
// destination rect.
static void drawImageFlipped(CGContextRef context,
                             const Rect& rect,
                             CGImageRef image)
{
    CGContextSaveGState(context);
    CGContextTranslateCTM(context, rect.x, rect.y + rect.h);
    CGContextScaleCTM(context, 1.0, -1.0);
    CGContextDrawImage(context, CGRectMake(0, 0, rect.w, rect.h), image);
    CGContextRestoreGState(context);
}

void MacOSContext::drawImage(const Image& image, const Rect& rect)
{
    if (!image.isValid())
        return;

    auto data = CFRef<CFDataRef>(
        CFDataCreate(nullptr,
                     image.pixels().data(),
                     static_cast<CFIndex>(image.pixels().size())));
    auto provider =
        CFRef<CGDataProviderRef>(CGDataProviderCreateWithCFData(data));
    if (!provider)
        return;

    auto colorSpace = getColorSpace();
    auto bitmapInfo = static_cast<std::uint32_t>(kCGImageAlphaLast)
                      | static_cast<std::uint32_t>(kCGBitmapByteOrder32Big);

    auto cgImage = CFRef<CGImageRef>(
        CGImageCreate(static_cast<std::size_t>(image.width()),
                      static_cast<std::size_t>(image.height()),
                      8,
                      32,
                      static_cast<std::size_t>(image.width()) * 4,
                      colorSpace,
                      bitmapInfo,
                      provider,
                      nullptr,
                      true,
                      kCGRenderingIntentDefault));
    if (!cgImage)
        return;

    drawImageFlipped(context, rect, cgImage);
}

} // namespace eacp::Graphics
