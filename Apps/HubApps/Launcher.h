#pragma once

// Cross-launch helpers shared by the Hub and the SecretPremiumApp. Each
// executable is built into its own directory next to the other under
// Apps/HubApps, so a sibling is found by walking up from argv[0].

#include <eacp/Core/Process/Process.h>

#include <filesystem>
#include <string>
#include <utility>

namespace hub
{

inline std::filesystem::path siblingExecutable(const char* argv0,
                                               const std::string& directory,
                                               const std::string& name)
{
    namespace fs = std::filesystem;

    // argv0 = .../HubApps/<thisApp>/<exe>; two levels up is .../HubApps.
    auto self = fs::absolute(fs::path {argv0});
    auto hubApps = self.parent_path().parent_path();

    auto executable = hubApps / directory / name;
#ifdef _WIN32
    executable += ".exe";
#endif
    return executable;
}

// Launches an executable detached, so it outlives this process. Returns
// false if the file is missing (e.g. the sibling target wasn't built).
inline bool launchDetached(const std::filesystem::path& executable,
                           std::vector<std::string> arguments = {})
{
    if (!std::filesystem::exists(executable))
        return false;

    auto options = eacp::Processes::ProcessOptions {};
    options.executable = executable.string();
    for (auto& argument: arguments)
        options.arguments.add(std::move(argument));
    options.detached = true;

    auto process = eacp::Processes::Process {std::move(options)};
    return process.launched();
}

} // namespace hub
