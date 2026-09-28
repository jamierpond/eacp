#pragma once

#include "Font.h"

#include <functional>

// The glyph source behind Font, TextMetrics and SoftwareContext on Android:
// TrueType files from /system/fonts, read by FreeType.
namespace eacp::Graphics::AndroidFonts
{
struct Face;

struct ResolvedFont final
{
    const Face* face = nullptr;
    float size = 12.f;
    bool syntheticBold = false;
};

// One glyph's 8-bit coverage at one pixel size, placed relative to the pen on
// the baseline in a y-down space.
struct GlyphImage final
{
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
    Vector<std::uint8_t> coverage;
};

// A line laid out at the font's size, in points: the pen position before each
// codepoint, and the byte in the UTF-8 source where each one starts.
struct GlyphRun final
{
    Vector<int> codepoints;
    Vector<int> byteOffsets;
    Vector<float> penPositions;
    float width = 0.f;
};

const ResolvedFont& resolve(const Font& font);

GlyphRun layout(const ResolvedFont& font, const std::string& text);

const GlyphImage& getGlyph(const ResolvedFont& font, int codepoint, float pixelSize);

float getAscent(const ResolvedFont& font);
float getDescent(const ResolvedFont& font);
float getLineGap(const ResolvedFont& font);

} // namespace eacp::Graphics::AndroidFonts
