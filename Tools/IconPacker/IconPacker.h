#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// Packs already-encoded PNGs into the native icon containers: .ico (Windows)
// and .icns (macOS). Both formats accept raw PNG payloads, so no image
// decoding is needed — only the IHDR dimensions are read.
namespace eacp::IconPacker
{
using Bytes = std::vector<uint8_t>;

inline void appendU16(Bytes& out, uint16_t value)
{
    out.push_back(static_cast<uint8_t>(value & 0xff));
    out.push_back(static_cast<uint8_t>(value >> 8));
}

inline void appendU32(Bytes& out, uint32_t value)
{
    appendU16(out, static_cast<uint16_t>(value & 0xffff));
    appendU16(out, static_cast<uint16_t>(value >> 16));
}

inline void appendU32BigEndian(Bytes& out, uint32_t value)
{
    out.push_back(static_cast<uint8_t>(value >> 24));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>(value & 0xff));
}

inline std::optional<uint32_t> getSquarePngSize(std::span<const uint8_t> png)
{
    constexpr auto signature =
        std::array<uint8_t, 8> {137, 80, 78, 71, 13, 10, 26, 10};

    if (png.size() < 24)
        return std::nullopt;

    if (!std::equal(signature.begin(), signature.end(), png.begin()))
        return std::nullopt;

    auto readU32BigEndian = [&](size_t offset)
    {
        return (static_cast<uint32_t>(png[offset]) << 24)
               | (static_cast<uint32_t>(png[offset + 1]) << 16)
               | (static_cast<uint32_t>(png[offset + 2]) << 8)
               | static_cast<uint32_t>(png[offset + 3]);
    };

    auto width = readU32BigEndian(16);
    auto height = readU32BigEndian(20);

    if (width == 0 || width != height)
        return std::nullopt;

    return width;
}

inline std::optional<Bytes> packIco(const std::vector<Bytes>& pngs)
{
    if (pngs.empty())
        return std::nullopt;

    auto out = Bytes {};
    appendU16(out, 0);
    appendU16(out, 1);
    appendU16(out, static_cast<uint16_t>(pngs.size()));

    auto offset = static_cast<uint32_t>(6 + 16 * pngs.size());

    for (const auto& png: pngs)
    {
        auto size = getSquarePngSize(png);
        if (!size || *size > 256)
            return std::nullopt;

        auto dimension = static_cast<uint8_t>(*size == 256 ? 0 : *size);
        out.push_back(dimension);
        out.push_back(dimension);
        out.push_back(0);
        out.push_back(0);
        appendU16(out, 1);
        appendU16(out, 32);
        appendU32(out, static_cast<uint32_t>(png.size()));
        appendU32(out, offset);

        offset += static_cast<uint32_t>(png.size());
    }

    for (const auto& png: pngs)
        out.insert(out.end(), png.begin(), png.end());

    return out;
}

inline std::optional<std::string_view> getIcnsTypeForSize(uint32_t size)
{
    switch (size)
    {
        case 16:
            return "icp4";
        case 32:
            return "icp5";
        case 64:
            return "icp6";
        case 128:
            return "ic07";
        case 256:
            return "ic08";
        case 512:
            return "ic09";
        case 1024:
            return "ic10";
        default:
            return std::nullopt;
    }
}

inline std::optional<Bytes> packIcns(const std::vector<Bytes>& pngs)
{
    if (pngs.empty())
        return std::nullopt;

    auto out = Bytes {'i', 'c', 'n', 's'};

    auto totalSize = static_cast<uint32_t>(8);
    for (const auto& png: pngs)
        totalSize += static_cast<uint32_t>(8 + png.size());

    appendU32BigEndian(out, totalSize);

    for (const auto& png: pngs)
    {
        auto size = getSquarePngSize(png);
        if (!size)
            return std::nullopt;

        auto type = getIcnsTypeForSize(*size);
        if (!type)
            return std::nullopt;

        out.insert(out.end(), type->begin(), type->end());
        appendU32BigEndian(out, static_cast<uint32_t>(8 + png.size()));
        out.insert(out.end(), png.begin(), png.end());
    }

    return out;
}
} // namespace eacp::IconPacker
