#pragma once

// A local-HTTP RPC server: an eacp::HTTP::Server with a Miro::Bridge mounted
// at /rpc, bound to an OS-assigned free port by default (see boundPort()).
// It also exposes a typed `call`, so a server can act as a client to other
// peers — which lets it talk to itself in a unit test.

#include <eacp/Network/HTTPRpc/RpcClient.h>
#include <eacp/Network/HTTPRpc/RpcServer.h>
#include <eacp/Network/HTTPServer/HttpServer.h>

#include <Miro/Miro.h>

#include <string>

namespace hub::rpc
{

class GatingServer
{
public:
    // port 0 (default) => the OS picks a free ephemeral port.
    explicit GatingServer(int port = 0);
    ~GatingServer();

    GatingServer(const GatingServer&) = delete;
    GatingServer& operator=(const GatingServer&) = delete;

    // Bind an API's reflected commands onto this server's bridge.
    template <typename Api>
    void serve(Api& api)
    {
        bridge.use(api);
    }

    int boundPort() const { return port; }
    std::string baseUrl() const;

    Miro::Bridge& getBridge() { return bridge; }

    // Client side — call any server (including ourselves). Constructs a
    // transient typed client; safe from any thread.
    template <typename Res, typename Req>
    Res call(const std::string& url,
             const std::string& command,
             const Req& request) const
    {
        return eacp::HTTP::Rpc::Client {url}.template invoke<Res>(command, request);
    }

    template <typename Res>
    Res call(const std::string& url, const std::string& command) const
    {
        return eacp::HTTP::Rpc::Client {url}.template invoke<Res>(command);
    }

private:
    Miro::Bridge bridge;
    eacp::HTTP::Server server;
    eacp::HTTP::Rpc::Server rpc {server, bridge};
    int port = -1;
};

} // namespace hub::rpc
