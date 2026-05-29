#include "HttpRange.h"

#include <algorithm>
#include <limits>

namespace eacp::Http
{
namespace
{
std::optional<std::uint64_t> parseSize(std::string_view value)
{
    if (value.empty())
        return std::nullopt;

    auto out = std::uint64_t {0};

    for (auto c: value)
    {
        if (c < '0' || c > '9')
            return std::nullopt;

        auto digit = static_cast<std::uint64_t>(c - '0');

        if (out > (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
            return std::nullopt;

        out = out * 10 + digit;
    }

    return out;
}
} // namespace

std::optional<ByteRange> parseByteRange(std::string_view header,
                                        std::uint64_t total)
{
    constexpr std::string_view prefix = "bytes=";

    if (total == 0 || header.rfind(prefix, 0) != 0
        || header.find(',') != std::string_view::npos)
        return std::nullopt;

    auto spec = header.substr(prefix.size());
    auto dash = spec.find('-');

    if (dash == std::string_view::npos)
        return std::nullopt;

    auto firstStr = spec.substr(0, dash);
    auto lastStr = spec.substr(dash + 1);

    auto start = std::uint64_t {0};
    auto end = std::uint64_t {0};

    if (firstStr.empty())
    {
        auto suffix = parseSize(lastStr);

        if (!suffix || *suffix == 0)
            return std::nullopt;

        auto clamped = std::min(*suffix, total);
        start = total - clamped;
        end = total - 1;
    }
    else
    {
        auto first = parseSize(firstStr);
        auto last = lastStr.empty() ? std::optional<std::uint64_t> {total - 1}
                                    : parseSize(lastStr);

        if (!first || !last)
            return std::nullopt;

        start = *first;
        end = *last;
    }

    if (start > end || start >= total)
        return std::nullopt;

    end = std::min(end, total - 1);
    return ByteRange {start, end};
}
} // namespace eacp::Http
