#include "ResponseBuilders.h"

namespace eacp::HTTP
{

Response makePlainTextResponse(int statusCode, const std::string& message)
{
    auto response = Response();
    response.statusCode = statusCode;
    response.setContent(message, "text/plain");
    return response;
}

} // namespace eacp::HTTP
