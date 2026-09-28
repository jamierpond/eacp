#include "Font-Android.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H
#include FT_SYNTHESIS_H

#include <algorithm>
#include <map>
#include <memory>
#include <tuple>

namespace eacp::Graphics::AndroidFonts
{
struct Face
{
    using GlyphKey = std::tuple<int, int, bool>;

    ~Face() { FT_Done_Face(face); }

    FT_Face face = nullptr;
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

struct Library final
{
    Library() { FT_Init_FreeType(&library); }
    ~Library() { FT_Done_FreeType(library); }

    FT_Library library = nullptr;
};

std::unique_ptr<Face> readFace(FT_Library library, const std::string& fileName)
{
    auto face = std::make_unique<Face>();
    auto path = "/system/fonts/" + fileName;

    if (!library || FT_New_Face(library, path.c_str(), 0, &face->face) != 0)
        return nullptr;

    face->unitsPerEm = float(std::max<FT_UShort>(face->face->units_per_EM, 1));
    return face;
}

const Face* loadFace(const std::string& fileName)
{
    // The library is built first, so it is torn down after the faces.
    static auto library = Library();
    static auto faces = std::map<std::string, std::unique_ptr<Face>>();

    auto found = faces.find(fileName);
    if (found == faces.end())
    {
        found = faces.emplace(fileName, readFace(library.library, fileName)).first;
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
// emboldened before rasterizing.
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

GlyphImage rasterize(const ResolvedFont& font, int codepoint, float pixelSize)
{
    auto* face = font.face->face;
    auto charSize = static_cast<FT_F26Dot6>(std::lround(pixelSize * 64.f));

    if (FT_Set_Char_Size(face, 0, charSize, 72, 72) != 0
        || FT_Load_Char(face, FT_ULong(codepoint), FT_LOAD_TARGET_LIGHT) != 0)
        return {};

    if (font.syntheticBold)
        FT_GlyphSlot_Embolden(face->glyph);

    if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_LIGHT) != 0)
        return {};

    const auto& bitmap = face->glyph->bitmap;
    auto glyph = GlyphImage();
    glyph.left = face->glyph->bitmap_left;
    glyph.top = -face->glyph->bitmap_top;
    glyph.width = int(bitmap.width);
    glyph.height = int(bitmap.rows);
    glyph.coverage.resize(glyph.width * glyph.height);

    for (int y = 0; y < glyph.height; ++y)
        std::copy_n(bitmap.buffer + y * bitmap.pitch,
                    glyph.width,
                    glyph.coverage.data() + y * glyph.width);

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

    auto* face = font.face->face;
    auto scale = font.size / font.face->unitsPerEm;
    auto kerning = FT_HAS_KERNING(face);
    auto pen = 0.f;
    auto previous = FT_UInt(0);

    for (int index = 0; index < (int) text.size();)
    {
        auto start = index;
        auto codepoint = decodeUtf8(text, index);

        auto glyph = FT_Get_Char_Index(face, FT_ULong(codepoint));

        if (kerning && previous != 0 && glyph != 0)
        {
            auto kern = FT_Vector();
            if (FT_Get_Kerning(face, previous, glyph, FT_KERNING_UNSCALED, &kern)
                == 0)
                pen += scale * float(kern.x);
        }

        auto advance = FT_Fixed(0);
        FT_Get_Advance(face, glyph, FT_LOAD_NO_SCALE, &advance);

        run.codepoints.add(codepoint);
        run.byteOffsets.add(start);
        run.penPositions.add(pen);

        pen += scale * float(advance);
        previous = glyph;
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
    return font.face
               ? font.size * float(font.face->face->ascender) / font.face->unitsPerEm
               : font.size * 0.8f;
}

float getDescent(const ResolvedFont& font)
{
    return font.face ? -font.size * float(font.face->face->descender)
                           / font.face->unitsPerEm
                     : font.size * 0.2f;
}

float getLineGap(const ResolvedFont& font)
{
    if (!font.face)
        return 0.f;

    auto* face = font.face->face;
    auto gap = face->height - (face->ascender - face->descender);
    return font.size * float(std::max<FT_Short>(gap, 0)) / font.face->unitsPerEm;
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
