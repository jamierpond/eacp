#include <IconPacker/IconPacker.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace eacp::IconPacker;

namespace
{
// A PNG signature plus an IHDR declaring the given dimensions. The packers
// never decode pixels, so no image data is needed.
Bytes makePng(uint32_t width, uint32_t height)
{
    auto png = Bytes {137, 80, 78, 71, 13, 10, 26, 10};

    appendU32BigEndian(png, 13);
    for (auto character: {'I', 'H', 'D', 'R'})
        png.push_back(static_cast<uint8_t>(character));

    appendU32BigEndian(png, width);
    appendU32BigEndian(png, height);
    png.insert(png.end(), {8, 6, 0, 0, 0});
    appendU32BigEndian(png, 0);

    return png;
}

uint32_t readU32LittleEndian(const Bytes& bytes, size_t offset)
{
    return static_cast<uint32_t>(bytes[offset])
           | (static_cast<uint32_t>(bytes[offset + 1]) << 8)
           | (static_cast<uint32_t>(bytes[offset + 2]) << 16)
           | (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

uint32_t readU32BigEndian(const Bytes& bytes, size_t offset)
{
    return (static_cast<uint32_t>(bytes[offset]) << 24)
           | (static_cast<uint32_t>(bytes[offset + 1]) << 16)
           | (static_cast<uint32_t>(bytes[offset + 2]) << 8)
           | static_cast<uint32_t>(bytes[offset + 3]);
}
} // namespace

auto tPngSizeParsing = test("IconPacker/pngSizeParsing") = []
{
    check(getSquarePngSize(makePng(256, 256)) == 256u);
    check(getSquarePngSize(makePng(16, 16)) == 16u);
    check(!getSquarePngSize(makePng(256, 128)));
    check(!getSquarePngSize(makePng(0, 0)));

    auto notPng = Bytes(32, 0);
    check(!getSquarePngSize(notPng));
};

auto tIcoLayout = test("IconPacker/icoLayout") = []
{
    auto small = makePng(16, 16);
    auto large = makePng(256, 256);
    auto ico = packIco({small, large});

    check(ico.has_value());
    check((*ico)[0] == 0 && (*ico)[1] == 0);
    check((*ico)[2] == 1 && (*ico)[3] == 0);
    check((*ico)[4] == 2 && (*ico)[5] == 0);

    check((*ico)[6] == 16);
    check((*ico)[6 + 16] == 0);

    auto firstOffset = readU32LittleEndian(*ico, 6 + 12);
    check(firstOffset == 6 + 16 * 2);
    check(readU32LittleEndian(*ico, 6 + 8) == small.size());

    auto secondOffset = readU32LittleEndian(*ico, 6 + 16 + 12);
    check(secondOffset == firstOffset + small.size());
    check(ico->size() == secondOffset + large.size());

    check(std::equal(small.begin(), small.end(), ico->begin() + firstOffset));
};

auto tIcoRejectsOversizedPng = test("IconPacker/icoRejectsOversizedPng") = []
{
    check(!packIco({makePng(512, 512)}));
    check(!packIco({}));
};

auto tIcnsLayout = test("IconPacker/icnsLayout") = []
{
    auto png = makePng(256, 256);
    auto icns = packIcns({png});

    check(icns.has_value());
    check((*icns)[0] == 'i' && (*icns)[1] == 'c' && (*icns)[2] == 'n'
          && (*icns)[3] == 's');
    check(readU32BigEndian(*icns, 4) == icns->size());

    check((*icns)[8] == 'i' && (*icns)[9] == 'c' && (*icns)[10] == '0'
          && (*icns)[11] == '8');
    check(readU32BigEndian(*icns, 12) == 8 + png.size());
    check(std::equal(png.begin(), png.end(), icns->begin() + 16));
};

auto tIcnsRejectsNonStandardSize = test("IconPacker/icnsRejectsNonStandardSize") = []
{
    check(!packIcns({makePng(100, 100)}));
    check(!packIcns({}));
};
