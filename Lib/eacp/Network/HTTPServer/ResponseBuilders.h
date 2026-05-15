#pragma once

#include <eacp/Network/HTTP/Http.h>

#include <string>

namespace eacp::HTTP
{

Response makePlainTextResponse(int statusCode, const std::string& message);

} // namespace eacp::HTTP
