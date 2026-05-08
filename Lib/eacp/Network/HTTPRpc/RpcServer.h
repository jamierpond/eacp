#pragma once

#include <eacp/Network/HTTPServer/HttpServer.h>

#include <Miro/Miro.h>

#include <functional>
#include <string>

namespace eacp::HTTP::Rpc
{

// Mounts a Miro::CommandTable onto an HTTP::Server as a single
// POST endpoint at `basePath` (default "/rpc"). Wire protocol mirrors
// the WebView bridge: request body is { "command": "...", "payload":
// {...} }, success response is { "result": <JSON> }, error response
// is the throwing handler's status code (or 404 for unknown commands)
// with body { "error": "..." }.
//
// The same MIRO_EXPORT_COMMAND set used by the WebView bridge can be
// installed via useStaticRegistry(), so a single set of typed handlers
// can be served over both transports.
//
// Lifetime: the RpcServer must outlive the HTTP::Server it was
// attached to — the registered route handler captures `this`.
class Server
{
public:
    Server(eacp::HTTP::Server& server, std::string basePath = "/rpc");

    template <typename Req, typename Res>
    void on(const std::string& command, std::function<Res(const Req&)> handler)
    {
        commands.on<Req, Res>(command, std::move(handler));
    }

    template <typename Req, typename Res>
    void on(const std::string& command, Res (*handler)(const Req&))
    {
        commands.on<Req, Res>(command, handler);
    }

    void useStaticRegistry()
    {
        Miro::CommandExport::registerStaticCommandsInto(commands);
    }

    Miro::CommandTable& commandTable() { return commands; }

private:
    Response handle(const Request& req);

    std::string basePath;
    Miro::CommandTable commands;
};

} // namespace eacp::HTTP::Rpc
