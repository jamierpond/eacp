#pragma once

// A bidirectional local-HTTP RPC peer.
//
// A Peer owns an eacp::HTTP::Server with a Miro::Bridge mounted at /rpc
// (bound to an OS-assigned free port by default), and can also act as a
// client to any other peer. It is symmetric: the same class serves its own
// API and calls out to others — so two Peers can talk inside one process
// (unit tests) exactly as two processes do over the loopback.
//
// The free-port problem is handled two ways:
//   * Peer(0) asks the OS for an ephemeral port and reports it via port().
//   * The Hub advertises its chosen port by writing baseUrl() to a
//     well-known rendezvous file; the app reads it (see the helpers below).

#include <eacp/Network/HTTPRpc/RpcClient.h>
#include <eacp/Network/HTTPRpc/RpcServer.h>
#include <eacp/Network/HTTPServer/HttpServer.h>

#include <Miro/Miro.h>

#include <optional>
#include <string>

namespace hub::rpc
{

class Peer
{
public:
    // port 0 (default) => the OS picks a free ephemeral port.
    explicit Peer(int port = 0);

    Peer(const Peer&) = delete;
    Peer& operator=(const Peer&) = delete;

    // Bind an API's reflected commands onto this peer's bridge.
    template <typename Api>
    void serve(Api& api)
    {
        bridge.use(api);
    }

    bool listening() const { return port >= 0; }
    int boundPort() const { return port; }
    std::string baseUrl() const;

    Miro::Bridge& getBridge() { return bridge; }

    // Client side — call any peer (including ourselves). Constructs a
    // transient typed client for the target URL; safe to call from any
    // thread.
    template <typename Res, typename Req>
    Res call(const std::string& peerUrl,
             const std::string& command,
             const Req& request) const
    {
        return eacp::HTTP::Rpc::Client {peerUrl}.template invoke<Res>(command,
                                                                      request);
    }

    template <typename Res>
    Res call(const std::string& peerUrl, const std::string& command) const
    {
        return eacp::HTTP::Rpc::Client {peerUrl}.template invoke<Res>(command);
    }

    ~Peer();

private:
    Miro::Bridge bridge;
    eacp::HTTP::Server server;
    eacp::HTTP::Rpc::Server rpc {server, bridge};
    int port = -1;
};

// Rendezvous file: an instance advertises its base URL under a name (e.g.
// "hub") so another process can find its free port. The path is a fixed,
// launch-method-independent location — otherwise a Finder-launched app and
// a terminal-launched one wouldn't agree.
std::string endpointPath(const std::string& name);
void writeEndpoint(const std::string& name, const std::string& baseUrl);
void removeEndpoint(const std::string& name);
std::optional<std::string> readEndpoint(const std::string& name);

// Single-instance guard: if a live instance advertised under `name` answers
// a `focus` call, ask it to raise its window and return true — the caller
// should then exit instead of opening a second window. Otherwise clears any
// stale endpoint and returns false.
bool focusRunningInstance(const std::string& name);

} // namespace hub::rpc
