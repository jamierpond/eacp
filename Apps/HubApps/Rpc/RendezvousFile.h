#pragma once

// File-based rendezvous between the two processes: a small well-known file
// through which one process advertises where to reach it, plus the same
// idea reused for a single-instance lock/focus handshake. Neither depends
// on the HTTP layer, so a pure client can use them without a server.

#include <optional>
#include <string>

namespace hub::rpc
{

// Rendezvous file: a server advertises its base URL under a name (e.g.
// "hub") so a client can find its free port. The path is a fixed,
// launch-method-independent location — otherwise a Finder-launched app and
// a terminal-launched one wouldn't agree.
std::string endpointPath(const std::string& name);
void writeEndpoint(const std::string& name, const std::string& baseUrl);
void removeEndpoint(const std::string& name);
std::optional<std::string> readEndpoint(const std::string& name);

// Single-instance guard, server-free so a pure client can use it too. An
// exclusive OS lock (flock / named mutex) marks the running instance; a
// second launch finds the lock held, drops a focus flag, and exits. The
// running instance polls focusRequested() and raises its window.
class SingleInstance
{
public:
    explicit SingleInstance(const std::string& name);
    ~SingleInstance();

    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    // True if we acquired the lock (we are the only instance).
    bool primary() const { return isPrimary; }

    // Called by a secondary instance to ask the primary to come forward.
    void requestFocus();

    // Polled by the primary (~5 Hz). True once per pending focus request.
    bool focusRequested();

private:
    std::string name;
    bool isPrimary = false;
#ifdef _WIN32
    void* mutexHandle = nullptr;
#else
    int lockFd = -1;
#endif
};

} // namespace hub::rpc
