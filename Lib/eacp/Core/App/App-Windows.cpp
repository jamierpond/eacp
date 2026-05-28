#include "../Utils/WinInclude.h"

#include "App.h"

#include <shellapi.h>
#include <winrt/base.h>

namespace eacp::Apps
{
void openExternalURL(const std::string& url)
{
    if (url.empty())
        return;

    auto wide = std::wstring(winrt::to_hstring(url));

    ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

// TODO: implement with IFileOpenDialog.
std::optional<std::string> chooseFile(const FilePickerOptions&)
{
    return std::nullopt;
}

// TODO: implement with IFileOpenDialog (FOS_PICKFOLDERS).
std::optional<std::string> chooseDirectory()
{
    return std::nullopt;
}
} // namespace eacp::Apps
