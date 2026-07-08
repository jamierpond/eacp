#include "Peer.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace hub::rpc
{

Peer::Peer(int requestedPort)
{
    // rpc (constructed above) has already registered the /rpc route on the
    // server; listen() snapshots the routes and binds the socket. A
    // requested port of 0 lets the OS assign a free one.
    if (!server.listen(requestedPort))
        throw std::runtime_error("Peer: failed to bind HTTP server");

    port = server.boundPort();
}

Peer::~Peer()
{
    // Stop accepting and drain the dispatcher before the bridge / rpc
    // members tear down, so no in-flight handler touches freed state.
    server.stop();
}

std::string Peer::baseUrl() const
{
    return "http://127.0.0.1:" + std::to_string(port) + "/rpc";
}

std::string endpointPath(const std::string& name)
{
    // A fixed location so every launch method (Finder, `open`, terminal)
    // resolves to the same file. /tmp is stable and world-local on
    // macOS/Linux; the system temp dir is the equivalent on Windows.
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

bool focusRunningInstance(const std::string& name)
{
    auto url = readEndpoint(name);
    if (!url)
        return false;

    try
    {
        // A live instance answers `focus` and raises its window; treat any
        // successful reply as "someone is already running".
        eacp::HTTP::Rpc::Client {*url}.invokeRaw(
            "focus", Miro::JSON {Miro::Json::Object {}});
        return true;
    }
    catch (const std::exception&)
    {
        // Stale endpoint (process gone / refused). Clear it and proceed.
        removeEndpoint(name);
        return false;
    }
}

} // namespace hub::rpc
