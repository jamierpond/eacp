#include "RendezvousFile.h"
#include "eacp/Core/Utils/Files.h"

#include <filesystem>
#include <fstream>

namespace hub::rpc
{

std::string endpointPath(const std::string& name)
{
    // A fixed, launch-method-independent location so every process (Finder,
    // `open`, terminal) agrees. /tmp is stable on macOS/Linux; the system
    // temp dir is the equivalent on Windows.
    auto dir = eacp::Files::getTemporaryDirectory();
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

} // namespace hub::rpc
