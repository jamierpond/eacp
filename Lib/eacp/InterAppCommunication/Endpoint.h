#pragma once

// File-based rendezvous between processes: a small well-known file through
// which a server advertises where to reach it, so a client started
// independently can find the server's (often OS-assigned) port. Doesn't depend
// on the HTTP layer, so a pure client can use it without pulling in a server.

#include <optional>
#include <string>

namespace eacp::Ipc
{

// A server advertises its base URL under a name (e.g. "hub"). The path is a
// fixed, launch-method-independent location — otherwise a Finder-launched app
// and a terminal-launched one wouldn't agree.
std::string endpointPath(const std::string& name);
void writeEndpoint(const std::string& name, const std::string& baseUrl);
void removeEndpoint(const std::string& name);
std::optional<std::string> readEndpoint(const std::string& name);

} // namespace eacp::Ipc
