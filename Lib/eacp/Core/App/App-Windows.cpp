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
} // namespace eacp::Apps
