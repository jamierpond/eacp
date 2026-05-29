#include "WebView.h"

#include <eacp/Core/Utils/Files.h>
#include <eacp/Core/Utils/Strings.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>

namespace eacp::Graphics
{
namespace
{
std::string pathFromFileURL(std::string_view url)
{
    auto schemeEnd = url.find("://");

    if (schemeEnd == std::string_view::npos)
        return {};

    auto rest = url.substr(schemeEnd + 3);
    auto cut = rest.find_first_of("?#");

    if (cut != std::string_view::npos)
        rest = rest.substr(0, cut);

    auto slash = rest.find('/');

    if (slash == std::string_view::npos)
        return {};

    return Strings::percentDecode(rest.substr(slash));
}

// A fresh stream per call: no shared cursor, so the concurrent, out-of-order
// range requests a media element issues cannot race each other.
std::optional<std::string> readRange(const std::string& path,
                                     std::uint64_t offset,
                                     std::uint64_t length)
{
    auto stream = std::ifstream {path, std::ios::binary};

    if (!stream)
        return std::nullopt;

    stream.seekg(static_cast<std::streamoff>(offset));

    if (!stream)
        return std::nullopt;

    auto buffer = std::string (length, '\0');
    stream.read(buffer.data(), static_cast<std::streamsize>(length));

    if (static_cast<std::uint64_t>(stream.gcount()) != length)
        return std::nullopt;

    return buffer;
}
} // namespace

ByteSourceResolver diskByteSource(std::vector<std::string> allowedRoots)
{
    return [roots = std::move(allowedRoots)](
               std::string_view url) -> std::optional<ByteSource>
    {
        auto pathStr = pathFromFileURL(url);

        if (pathStr.empty())
            return std::nullopt;

        auto ec = std::error_code {};
        auto path = std::filesystem::weakly_canonical(pathStr, ec);

        if (ec)
            path = std::filesystem::path {pathStr};

        auto allowed = roots.empty()
                    || std::any_of(roots.begin(), roots.end(),
                                   [&](const auto& root)
                                   { return Files::isUnderRoot(path, root); });

        if (!allowed || !std::filesystem::is_regular_file(path, ec))
            return std::nullopt;

        auto size = std::filesystem::file_size(path, ec);

        if (ec)
            return std::nullopt;

        auto pathString = path.string();

        return ByteSource {
            static_cast<std::uint64_t>(size),
            mimeForPath(pathString),
            [pathString](std::uint64_t offset, std::uint64_t length)
            { return readRange(pathString, offset, length); }};
    };
}
} // namespace eacp::Graphics
