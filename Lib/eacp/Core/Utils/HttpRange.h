#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace eacp::Http
{
struct ByteRange
{
    std::uint64_t start = 0;  // inclusive
    std::uint64_t end = 0;    // inclusive
};

// Resolves a single HTTP `Range` header against a known total size. Handles
// `bytes=a-b`, `bytes=a-`, and suffix `bytes=-n`. Returns nullopt (caller
// serves the whole body) for anything absent, malformed, multi-range, or
// unsatisfiable.
std::optional<ByteRange> parseByteRange(std::string_view header,
                                        std::uint64_t total);
} // namespace eacp::Http
