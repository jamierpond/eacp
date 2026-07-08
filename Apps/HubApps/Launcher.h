#pragma once

// Cross-launch helpers shared by the Hub and the SecretPremiumApp. Each
// executable is built under Apps/HubApps/<TargetDir>/, as a plain binary
// on Windows and a .app bundle on macOS, so a sibling is found by walking
// up to the shared "HubApps" directory and back down.

#include <eacp/Core/Process/Process.h>

#include <filesystem>
#include <string>
#include <utility>

namespace hub
{

// Walk up from argv[0] to the shared HubApps directory. Works whether the
// caller is a bare binary (.../HubApps/Hub/Hub) or a macOS bundle
// (.../HubApps/Hub/Hub.app/Contents/MacOS/Hub).
inline std::filesystem::path hubAppsRoot(const char* argv0)
{
    namespace fs = std::filesystem;

    auto self = fs::absolute(fs::path {argv0});
    for (auto dir = self; dir.has_parent_path(); dir = dir.parent_path())
        if (dir.filename() == "HubApps")
            return dir;

    // Fallback for the bare-binary layout: .../HubApps/<dir>/<exe>.
    return self.parent_path().parent_path();
}

inline std::filesystem::path siblingExecutable(const char* argv0,
                                               const std::string& directory,
                                               const std::string& name)
{
    auto base = hubAppsRoot(argv0) / directory;

#if defined(__APPLE__)
    auto bundled = base / (name + ".app") / "Contents" / "MacOS" / name;
    if (std::filesystem::exists(bundled))
        return bundled;
#endif

    auto plain = base / name;
#ifdef _WIN32
    plain += ".exe";
#endif
    return plain;
}

// Launches an executable detached, so it outlives this process. Returns
// false if the file is missing (e.g. the sibling target wasn't built).
inline bool launchDetached(const std::filesystem::path& executable)
{
    if (!std::filesystem::exists(executable))
        return false;

    auto options = eacp::Processes::ProcessOptions {};
    options.executable = executable.string();
    options.detached = true;

    auto process = eacp::Processes::Process {std::move(options)};
    return process.launched();
}

} // namespace hub
