#pragma once

// A tiny nng-backed transport for a Miro::Bridge. Two channels:
//
//   RpcServer / RpcClient   request/reply over nng REQ/REP, carrying the
//                           same {command,payload} -> {result}|{error}
//                           envelope the eacp HTTP RPC uses. Miro does
//                           the (de)serialisation; nng moves the bytes.
//
//   Publisher / Subscriber  fire-and-forget events over nng PUB/SUB,
//                           carrying {event,payload}. The Publisher hangs
//                           off Bridge::onEmit, so any api.event.publish()
//                           fans out to every subscriber.

#include <Miro/Miro.h>

#include <ea_data_structures/Pointers/Broadcaster.h>

#include <nng/nng.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace hub::ipc
{

using Invoke =
    std::function<Miro::JSON(const std::string& command, const Miro::JSON& payload)>;

// Binds a Bridge to an nng REP socket and services requests on its own
// thread until destroyed.
class RpcServer
{
public:
    RpcServer(Miro::Bridge& bridge, const std::string& url);
    ~RpcServer();

    RpcServer(const RpcServer&) = delete;
    RpcServer& operator=(const RpcServer&) = delete;

private:
    void receiveLoop();

    Miro::Bridge& bridge;
    nng_socket socket {};
    std::atomic<bool> running {true};
    std::thread worker;
};

// Typed REQ client. Dials lazily (non-blocking), so it can be built
// before the Hub exists; the first call blocks up to the receive timeout
// while the dialer connects.
class RpcClient
{
public:
    explicit RpcClient(const std::string& url);
    ~RpcClient();

    RpcClient(const RpcClient&) = delete;
    RpcClient& operator=(const RpcClient&) = delete;

    void setTimeoutMs(int milliseconds);

    template <typename Res, typename Req>
    Res invoke(const std::string& command, const Req& request) const
    {
        auto out = Res {};
        Miro::fromJSON(out, invokeRaw(command, Miro::toJSON(request)));
        return out;
    }

    template <typename Res>
    Res invoke(const std::string& command) const
    {
        auto out = Res {};
        Miro::fromJSON(out, invokeRaw(command, Miro::JSON {Miro::Json::Object {}}));
        return out;
    }

    Miro::JSON invokeRaw(const std::string& command,
                         const Miro::JSON& payload) const;

    Invoke asInvoker() const;

private:
    nng_socket socket {};
};

// Broadcasts every Bridge emit over an nng PUB socket as {event,payload}.
class Publisher
{
public:
    Publisher(Miro::Bridge& bridge, const std::string& url);
    ~Publisher();

    Publisher(const Publisher&) = delete;
    Publisher& operator=(const Publisher&) = delete;

private:
    void onEmit();

    Miro::Bridge& bridge;
    nng_socket socket {};
    std::mutex sendMutex;
    EA::Listener emitListener; // declared last: attaches after members exist
};

// Subscribes to a PUB socket and delivers decoded {event,payload}
// messages to `handler` on its own thread.
class Subscriber
{
public:
    using Handler =
        std::function<void(const std::string& event, const Miro::JSON& payload)>;

    Subscriber(const std::string& url, Handler handler);
    ~Subscriber();

    Subscriber(const Subscriber&) = delete;
    Subscriber& operator=(const Subscriber&) = delete;

private:
    void receiveLoop();

    Handler handler;
    nng_socket socket {};
    std::atomic<bool> running {true};
    std::thread worker;
};

} // namespace hub::ipc
