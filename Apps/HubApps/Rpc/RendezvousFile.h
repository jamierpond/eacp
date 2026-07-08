#pragma once

// File-based rendezvous between the two processes: a small well-known file
// through which the Hub advertises where to reach it, so a client started
// independently can find the Hub's (free) port. Doesn't depend on the HTTP
// layer, so a pure client can use it without pulling in a server.

#include <optional>
#include <string>

{

// A server advertises its base URL under a name (e.g. "hub"). The path is a
// fixed, launch-method-independent location — otherwise a Finder-launched
// app and a terminal-launched one wouldn't agree.
std::string endpointPath(const std::string& name);
void writeEndpoint(const std::string& name, const std::string& baseUrl);
void removeEndpoint(const std::string& name);
std::optional<std::string> readEndpoint(const std::string& name);

} // namespace hub::rpc
