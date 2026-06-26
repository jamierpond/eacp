#include "Clipboard.h"

#include <eacp/Core/Utils/WinInclude.h>

#include <shellapi.h>

#include <cstring>
#include <vector>

namespace eacp::Clipboard
{
namespace
{
std::wstring toWideString(std::string_view text)
{
    if (text.empty())
        return {};

    auto required = MultiByteToWideChar(CP_UTF8,
                                        0,
                                        text.data(),
                                        static_cast<int>(text.size()),
                                        nullptr,
                                        0);
    if (required <= 0)
        return {};

    auto result = std::wstring(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8,
                        0,
                        text.data(),
                        static_cast<int>(text.size()),
                        result.data(),
                        required);
    return result;
}
} // namespace

bool copyText(std::string_view text)
{
    auto wide = toWideString(text);
    auto bytes = (wide.size() + 1) * sizeof(wchar_t);
    auto handle = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!handle)
        return false;

    auto* data = static_cast<wchar_t*>(GlobalLock(handle));
    if (!data)
    {
        GlobalFree(handle);
        return false;
    }

    std::memcpy(data, wide.c_str(), bytes);
    GlobalUnlock(handle);

    if (!OpenClipboard(nullptr))
    {
        GlobalFree(handle);
        return false;
    }

    EmptyClipboard();
    auto ok = SetClipboardData(CF_UNICODETEXT, handle) != nullptr;
    CloseClipboard();

    if (!ok)
        GlobalFree(handle);

    return ok;
}

bool copyFiles(const Vector<std::string>& paths)
{
    if (paths.empty())
        return false;

    auto widePaths = std::vector<std::wstring> {};
    auto pathChars = std::size_t {1}; // final extra null terminator

    for (const auto& path: paths)
    {
        auto wide = toWideString(path);
        if (wide.empty())
            continue;

        pathChars += wide.size() + 1;
        widePaths.push_back(std::move(wide));
    }

    if (widePaths.empty())
        return false;

    auto bytes = sizeof(DROPFILES) + pathChars * sizeof(wchar_t);
    auto handle = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
    if (!handle)
        return false;

    auto* dropFiles = static_cast<DROPFILES*>(GlobalLock(handle));
    if (!dropFiles)
    {
        GlobalFree(handle);
        return false;
    }

    dropFiles->pFiles = sizeof(DROPFILES);
    dropFiles->fWide = TRUE;

    auto* cursor = reinterpret_cast<wchar_t*>(
        reinterpret_cast<char*>(dropFiles) + sizeof(DROPFILES));
    for (const auto& path: widePaths)
    {
        std::memcpy(cursor, path.c_str(), path.size() * sizeof(wchar_t));
        cursor += path.size() + 1;
    }

    GlobalUnlock(handle);

    if (!OpenClipboard(nullptr))
    {
        GlobalFree(handle);
        return false;
    }

    EmptyClipboard();
    auto ok = SetClipboardData(CF_HDROP, handle) != nullptr;
    CloseClipboard();

    if (!ok)
        GlobalFree(handle);

    return ok;
}
} // namespace eacp::Clipboard
