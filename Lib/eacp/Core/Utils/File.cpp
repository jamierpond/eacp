#include "File.h"

namespace eacp
{
File::File(std::filesystem::path path)
    : filePath(std::move(path))
{
}

bool File::exists() const
{
    auto ec = std::error_code {};
    return std::filesystem::exists(filePath, ec);
}

bool File::isRegularFile() const
{
    auto ec = std::error_code {};
    return std::filesystem::is_regular_file(filePath, ec);
}

// Canonicalise both sides so the check is symlink-consistent (e.g. macOS
// /var -> /private/var) regardless of whether the caller pre-normalised. A
// path that escapes the root resolves to a relative path starting with "..";
// anything else (including ".") is contained.
bool File::isUnder(const std::filesystem::path& root) const
{
    auto ec = std::error_code {};
    auto canonicalRoot = std::filesystem::weakly_canonical(root, ec);
    auto canonicalFile = std::filesystem::weakly_canonical(filePath, ec);
    auto rel = std::filesystem::relative(canonicalFile, canonicalRoot, ec);

    if (ec || rel.empty())
        return false;

    return rel.generic_string().rfind("..", 0) != 0;
}

std::uint64_t File::size() const
{
    auto ec = std::error_code {};
    auto bytes = std::filesystem::file_size(filePath, ec);
    return ec ? 0 : static_cast<std::uint64_t>(bytes);
}

bool File::openForRead()
{
    if (stream.is_open())
        return true;

    stream.open(filePath, std::ios::binary);
    position = 0;
    return stream.is_open();
}

// Seeks clear the stream state first: a previous read may have left EOF/fail
// bits set, and seekg would fail while they are latched.
std::size_t File::read(std::uint64_t offset, std::span<std::uint8_t> out)
{
    if (!openForRead() || out.empty())
        return 0;

    if (offset != position)
    {
        stream.clear();
        stream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        position = offset;
    }

    stream.read(reinterpret_cast<char*>(out.data()),
                static_cast<std::streamsize>(out.size()));

    auto got = static_cast<std::size_t>(stream.gcount());
    position += got;
    return got;
}
} // namespace eacp
