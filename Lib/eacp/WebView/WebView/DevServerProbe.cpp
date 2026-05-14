#include "DevServerProbe.h"

#include "DevServerProbeInternal.h"

#include <optional>

namespace eacp::Graphics
{
namespace
{
struct HostPort
{
    std::string host;
    int port = 0;
};

std::optional<HostPort> parseHostPort(const std::string& url)
{
    auto schemeEnd = url.find("://");

    if (schemeEnd == std::string::npos)
        return std::nullopt;

    auto hostStart = schemeEnd + 3;
    auto pathStart = url.find('/', hostStart);
    auto hostPart = url.substr(
        hostStart,
        pathStart == std::string::npos ? std::string::npos
                                       : pathStart - hostStart);

    auto colon = hostPart.find(':');
    auto result = HostPort {};

    if (colon != std::string::npos)
    {
        result.host = hostPart.substr(0, colon);

        try
        {
            result.port = std::stoi(hostPart.substr(colon + 1));
        }
        catch (...)
        {
            return std::nullopt;
        }
    }
    else
    {
        result.host = hostPart;
        result.port = url.starts_with("https://") ? 443 : 80;
    }

    return result;
}
} // namespace

bool probeDevServer(const std::string& url, int timeoutMs)
{
    auto hp = parseHostPort(url);

    if (! hp)
        return false;

    return probeTCP(hp->host, hp->port, timeoutMs);
}
} // namespace eacp::Graphics
