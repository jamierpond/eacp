#include "Font-Android.h"

#include <stb_truetype.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <memory>
#include <tuple>

namespace eacp::Graphics::AndroidFonts
{
struct Face
{
    using GlyphKey = std::tuple<int, int, bool>;

    Vector<std::uint8_t> bytes;
    stbtt_fontinfo info {};
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    float unitsPerEm = 1000.f;
    mutable std::map<GlyphKey, GlyphImage> glyphs;
};
} // namespace eacp::Graphics::AndroidFonts

namespace eacp::Graphics
{
namespace
{
using AndroidFonts::Face;
using AndroidFonts::GlyphImage;
using AndroidFonts::ResolvedFont;

std::unique_ptr<Face> readFace(const std::string& fileName)
{
    auto stream = std::ifstream("/system/fonts/" + fileName, std::ios::binary);
    if (!stream)
        return nullptr;

    auto face = std::make_unique<Face>();
    stream.seekg(0, std::ios::end);
    face->bytes.resize(static_cast<int>(stream.tellg()));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(face->bytes.data()), face->bytes.size());

    auto* data = face->bytes.data();
    if (!stream
        || !stbtt_InitFont(&face->info, data, stbtt_GetFontOffsetForIndex(data, 0)))
        return nullptr;

    stbtt_GetFontVMetrics(
        &face->info, &face->ascent, &face->descent, &face->lineGap);
    face->unitsPerEm = 1.f / stbtt_ScaleForMappingEmToPixels(&face->info, 1.f);
    return face;
}

const Face* loadFace(const std::string& fileName)
{
    static auto faces = std::map<std::string, std::unique_ptr<Face>>();

    auto found = faces.find(fileName);
    if (found == faces.end())
    {
        found = faces.emplace(fileName, readFace(fileName)).first;
        if (!found->second)
            LOG("eacp: no font at /system/fonts/", fileName);
    }

    return found->second.get();
}

const Face* loadFirstFace(std::initializer_list<const char*> fileNames)
{
    for (auto* fileName: fileNames)
        if (auto* face = loadFace(fileName))
            return face;

    return nullptr;
}

bool containsAny(const std::string& text, std::initializer_list<const char*> words)
{
    return std::any_of(words.begin(),
                       words.end(),
                       [&](const char* word)
                       { return text.find(word) != std::string::npos; });
}

// Android ships no bold monospace, so a bold mono name gets its regular face
// thickened after rasterizing (see embolden).
ResolvedFont resolveOptions(const FontOptions& options)
{
    auto name = options.name;
    std::transform(name.begin(),
                   name.end(),
                   name.begin(),
                   [](unsigned char c)
                   { return static_cast<char>(std::tolower(c)); });

    auto bold = containsAny(name, {"bold"});
    auto font = ResolvedFont {nullptr, options.size, false};

    if (containsAny(name, {"menlo", "mono", "courier"}))
    {
        font.face = loadFirstFace({"DroidSansMono.ttf"});
        font.syntheticBold = bold;
    }
    else if (bold)
    {
        font.face = loadFirstFace({"DroidSans-Bold.ttf"});
    }

    if (!font.face)
    {
        font.face = loadFirstFace({"Roboto-Regular.ttf", "DroidSans.ttf"});
        font.syntheticBold = bold;
    }

    return font;
}

int decodeUtf8(const std::string& text, int& index)
{
    auto byte = [&](int at)
    { return static_cast<unsigned char>(text[(size_t) at]); };

    auto first = byte(index++);
    auto extra = first >= 0xF0 ? 3 : first >= 0xE0 ? 2 : first >= 0xC0 ? 1 : 0;
    auto codepoint = extra == 0 ? int(first) : int(first & (0x3F >> extra));

    for (; extra > 0 && index < (int) text.size() && (byte(index) & 0xC0) == 0x80;
         --extra)
        codepoint = (codepoint << 6) | (byte(index++) & 0x3F);

    return extra == 0 ? codepoint : 0xFFFD;
}

// Smears the coverage rightwards by radius pixels, keeping the brightest
// sample: the look of a glyph drawn radius+1 times one pixel apart.
void embolden(GlyphImage& glyph, int radius)
{
    auto width = glyph.width + radius;
    auto thick = Vector<std::uint8_t>(width * glyph.height);

    for (int y = 0; y < glyph.height; ++y)
        for (int x = 0; x < width; ++x)
        {
            auto value = std::uint8_t {0};
            for (int k = 0; k <= radius; ++k)
                if (x - k >= 0 && x - k < glyph.width)
                    value = std::max(value, glyph.coverage[y * glyph.width + x - k]);
            thick[y * width + x] = value;
        }

    glyph.width = width;
    glyph.coverage = std::move(thick);
}

GlyphImage rasterize(const ResolvedFont& font, int codepoint, float pixelSize)
{
    auto glyph = GlyphImage();
    auto* info = &font.face->info;
    auto scale = stbtt_ScaleForMappingEmToPixels(info, pixelSize);

    auto* bitmap = stbtt_GetCodepointBitmap(info,
                                            scale,
                                            scale,
                                            codepoint,
                                            &glyph.width,
                                            &glyph.height,
                                            &glyph.left,
                                            &glyph.top);
    if (!bitmap)
        return {};

    glyph.coverage.assign(bitmap, bitmap + glyph.width * glyph.height);
    stbtt_FreeBitmap(bitmap, nullptr);

    if (font.syntheticBold)
        embolden(glyph, std::max(1, int(std::lround(pixelSize / 24.f))));

    return glyph;
}
} // namespace

namespace AndroidFonts
{
const ResolvedFont& resolve(const Font& font)
{
    return *static_cast<const ResolvedFont*>(font.getHandle());
}

GlyphRun layout(const ResolvedFont& font, const std::string& text)
{
    auto run = GlyphRun();
    if (!font.face)
        return run;

    auto* info = &font.face->info;
    auto scale = font.size / font.face->unitsPerEm;
    auto pen = 0.f;
    auto previous = 0;

    for (int index = 0; index < (int) text.size();)
    {
        auto start = index;
        auto codepoint = decodeUtf8(text, index);

        if (previous != 0)
            pen += scale
                   * float(stbtt_GetCodepointKernAdvance(info, previous, codepoint));

        auto advance = 0;
        auto bearing = 0;
        stbtt_GetCodepointHMetrics(info, codepoint, &advance, &bearing);

        run.codepoints.add(codepoint);
        run.byteOffsets.add(start);
        run.penPositions.add(pen);

        pen += scale * float(advance);
        previous = codepoint;
    }

    run.width = pen;
    return run;
}

const GlyphImage& getGlyph(const ResolvedFont& font, int codepoint, float pixelSize)
{
    auto key = Face::GlyphKey {
        codepoint, int(std::lround(pixelSize * 4.f)), font.syntheticBold};
    auto& glyphs = font.face->glyphs;

    auto found = glyphs.find(key);
    if (found == glyphs.end())
        found = glyphs.emplace(key, rasterize(font, codepoint, pixelSize)).first;

    return found->second;
}

float getAscent(const ResolvedFont& font)
{
    return font.face ? font.size * float(font.face->ascent) / font.face->unitsPerEm
                     : font.size * 0.8f;
}

float getDescent(const ResolvedFont& font)
{
    return font.face ? -font.size * float(font.face->descent) / font.face->unitsPerEm
                     : font.size * 0.2f;
}

float getLineGap(const ResolvedFont& font)
{
    return font.face ? font.size * float(font.face->lineGap) / font.face->unitsPerEm
                     : 0.f;
}
} // namespace AndroidFonts

struct Font::Native
{
    ResolvedFont resolved;
};

Font::Font(const FontOptions& optionsToUse)
    : options(optionsToUse)
    , impl()
{
    updateNativeFont();
}

void Font::setFont(const FontOptions& optionsToUse)
{
    options = optionsToUse;
    updateNativeFont();
}

void* Font::getHandle() const
{
    return const_cast<ResolvedFont*>(&impl->resolved);
}

void Font::updateNativeFont()
{
    impl->resolved = resolveOptions(options);
}

} // namespace eacp::Graphics
