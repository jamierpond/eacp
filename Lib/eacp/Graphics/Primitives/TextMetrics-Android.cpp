#include "TextMetrics.h"
#include "Font-Android.h"

namespace eacp::Graphics
{
float TextMetrics::measureWidth(const std::string& text, const Font& font)
{
    return AndroidFonts::layout(AndroidFonts::resolve(font), text).width;
}

float TextMetrics::getOffsetForIndex(const std::string& text,
                                     int index,
                                     const Font& font)
{
    if (index <= 0)
        return 0.f;

    return measureWidth(text.substr(0, (size_t) index), font);
}

int TextMetrics::getIndexForOffset(const std::string& text,
                                   float xOffset,
                                   const Font& font)
{
    auto run = AndroidFonts::layout(AndroidFonts::resolve(font), text);

    for (int i = 0; i < run.codepoints.size(); ++i)
    {
        auto next =
            i + 1 < run.codepoints.size() ? run.penPositions[i + 1] : run.width;
        if (xOffset < (run.penPositions[i] + next) * 0.5f)
            return run.byteOffsets[i];
    }

    return (int) text.size();
}

float TextMetrics::getLineHeight(const Font& font)
{
    auto& resolved = AndroidFonts::resolve(font);
    return AndroidFonts::getAscent(resolved) + AndroidFonts::getDescent(resolved)
           + AndroidFonts::getLineGap(resolved);
}

float TextMetrics::getAscent(const Font& font)
{
    return AndroidFonts::getAscent(AndroidFonts::resolve(font));
}

float TextMetrics::getDescent(const Font& font)
{
    return AndroidFonts::getDescent(AndroidFonts::resolve(font));
}

} // namespace eacp::Graphics
