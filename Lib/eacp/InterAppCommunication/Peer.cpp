#include "Peer.h"

#include <stdexcept>

namespace eacp::Ipc
{

Peer::Peer(int requestedPort)
{
    // rpc (constructed above) has already registered the /rpc route on the
    // server; listen() snapshots the routes and binds the socket. A requested
    // port of 0 lets the OS assign a free one.
    if (!server.listen(requestedPort))
        throw std::runtime_error("Ipc::Peer: failed to bind HTTP server");

    port = server.boundPort();
}

Peer::~Peer()
{
    // Stop accepting and drain the dispatcher before the bridge / rpc members
    // tear down, so no in-flight handler touches freed state.
    server.stop();
}

std::string Peer::baseUrl() const
{
    return "http://127.0.0.1:" + std::to_string(port) + "/rpc";
}

} // namespace eacp::Ipc
