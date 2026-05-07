#include "HttpServer.h"

namespace eacp::HTTP
{

namespace
{
Response makePlainTextResponse(int status, const std::string& message)
{
    auto res = Response();
    res.statusCode = status;
    res.setContent(message, "text/plain");
    return res;
}

std::string routeKey(const std::string& method, const std::string& path)
{
    return method + " " + path;
}
} // namespace

Error::Error(int statusCodeToUse, const std::string& message)
    : std::runtime_error(message)
    , statusCode(statusCodeToUse)
    , response(makePlainTextResponse(statusCodeToUse, message))
{
}

Error::Error(Response responseToSend)
    : std::runtime_error(responseToSend.content)
    , statusCode(responseToSend.statusCode)
    , response(std::move(responseToSend))
{
}

void throwError(const std::string& message, int statusCode)
{
    throw Error(statusCode, message);
}

void Server::addRoute(const std::string& method,
                      const std::string& path,
                      RequestHandler handler)
{
    routes[routeKey(method, path)] = std::move(handler);
}

void Server::get(const std::string& path, RequestHandler handler)
{
    addRoute("GET", path, std::move(handler));
}

void Server::post(const std::string& path, RequestHandler handler)
{
    addRoute("POST", path, std::move(handler));
}

void Server::put(const std::string& path, RequestHandler handler)
{
    addRoute("PUT", path, std::move(handler));
}

void Server::del(const std::string& path, RequestHandler handler)
{
    addRoute("DELETE", path, std::move(handler));
}

bool Server::listen(int port)
{
    return listen(port, buildRouteHandler());
}

RequestHandler Server::buildRouteHandler() const
{
    auto routesCopy = routes;

    return [routesCopy](const Request& req) -> Response
    {
        try
        {
            auto it = routesCopy.find(routeKey(req.type, req.pathWithoutQuery()));

            if (it == routesCopy.end())
                return makePlainTextResponse(404, "Not Found");

            return it->second(req);
        }
        catch (const Error& e)
        {
            return e.response;
        }
        catch (const std::exception& e)
        {
            return makePlainTextResponse(500, e.what());
        }
    };
}

} // namespace eacp::HTTP
