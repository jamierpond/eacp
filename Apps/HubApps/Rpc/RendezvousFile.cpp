#include "RendezvousFile.h"

#include <filesystem>
#include <fstream>

namespace hub::rpc
{

std::string endpointPath(const std::string& name)
{
    // A fixed, launch-method-independent location so every process (Finder,
    // `open`, terminal) agrees. /tmp is stable on macOS/Linux; the system
    // temp dir is the equivalent on Windows.
#ifdef _WIN32
    auto dir = std::filesystem::temp_directory_path();
#else
    auto dir = std::filesystem::path {"/tmp"};
#endif
    return (dir / ("eacp-" + name + ".endpoint")).string();
}

void writeEndpoint(const std::string& name, const std::string& baseUrl)
{
    auto out = std::ofstream {endpointPath(name), std::ios::trunc};
    out << baseUrl;
}

void removeEndpoint(const std::string& name)
{
    auto error = std::error_code {};
    std::filesystem::remove(endpointPath(name), error);
}

std::optional<std::string> readEndpoint(const std::string& name)
{
    auto in = std::ifstream {endpointPath(name)};
    if (!in)
        return std::nullopt;

    auto url = std::string {};
    std::getline(in, url);

    if (url.empty())
        return std::nullopt;

    return url;
}

SingleInstance::SingleInstance(const std::string& nameToUse)
    : name(nameToUse)
{
#ifdef _WIN32
    auto wide = std::wstring(name.begin(), name.end());
    mutexHandle = CreateMutexW(nullptr, TRUE, (L"eacp-" + wide).c_str());
    isPrimary = mutexHandle != nullptr && GetLastError() != ERROR_ALREADY_EXISTS;
#else
    // An advisory lock the OS releases automatically if we die — no stale
    // lock to clean up, unlike a PID file.
    lockFd = ::open(tempPath(name, ".lock").c_str(), O_CREAT | O_RDWR, 0644);
    isPrimary = lockFd >= 0 && ::flock(lockFd, LOCK_EX | LOCK_NB) == 0;
    if (lockFd >= 0 && !isPrimary)
    {
        ::close(lockFd);
        lockFd = -1;
    }
#endif
}

SingleInstance::~SingleInstance()
{
#ifdef _WIN32
    if (mutexHandle != nullptr)
        CloseHandle(mutexHandle);
#else
    if (lockFd >= 0)
    {
        ::flock(lockFd, LOCK_UN);
        ::close(lockFd);
    }
#endif
    if (isPrimary)
    {
        auto error = std::error_code {};
        std::filesystem::remove(tempPath(name, ".lock"), error);
        std::filesystem::remove(tempPath(name, ".focus"), error);
    }
}

void SingleInstance::requestFocus()
{
    auto out = std::ofstream {tempPath(name, ".focus"), std::ios::trunc};
    out << "1";
}

bool SingleInstance::focusRequested()
{
    auto focusPath = tempPath(name, ".focus");
    if (!std::filesystem::exists(focusPath))
        return false;

    auto error = std::error_code {};
    std::filesystem::remove(focusPath, error);
    return true;
}

} // namespace hub::rpc
