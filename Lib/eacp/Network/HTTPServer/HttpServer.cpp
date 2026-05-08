#include "HttpServer.h"
#include "HttpServerDispatcher.h"

namespace eacp::HTTP
{

const char* reasonPhrase(int code)
{
    switch (code)
    {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

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
