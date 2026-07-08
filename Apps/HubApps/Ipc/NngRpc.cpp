#include "NngRpc.h"

#include <nng/protocol/pubsub0/pub.h>
#include <nng/protocol/pubsub0/sub.h>
#include <nng/protocol/reqrep0/rep.h>
#include <nng/protocol/reqrep0/req.h>

#include <stdexcept>
#include <utility>

namespace hub::ipc
{
namespace
{

void check(int result, const char* what)
{
    if (result != 0)
        throw std::runtime_error(std::string {what} + ": " + nng_strerror(result));
}

// nng REQ/REP round-trips raw bytes; we frame them as the Miro RPC
// envelope so a Bridge can be driven unchanged.
Miro::JSON makeEnvelope(const std::string& command, const Miro::JSON& payload)
{
    auto envelope = Miro::JSON {Miro::Json::Object {}};
    envelope.asObject()["command"] = Miro::JSON {command};
    envelope.asObject()["payload"] = payload;
    return envelope;
}

std::string dispatchEnvelope(Miro::Bridge& bridge, const std::string& requestText)
{
    auto reply = Miro::JSON {Miro::Json::Object {}};

    try
    {
        auto request = Miro::Json::parse(requestText);
        auto& object = request.asObject();
        auto command = object.at("command").asString();

        auto payloadIt = object.find("payload");
        auto payload = payloadIt != object.end()
                           ? payloadIt->second
                           : Miro::JSON {Miro::Json::Object {}};

        reply.asObject()["result"] = bridge.dispatch(command, payload);
    }
    catch (const std::exception& error)
    {
        reply.asObject()["error"] = Miro::JSON {std::string {error.what()}};
    }

    return Miro::Json::print(reply);
}

void sendString(nng_socket socket, const std::string& text)
{
    check(nng_send(socket, const_cast<char*>(text.data()), text.size(), 0),
          "nng_send");
}

// Blocking receive into a std::string. Returns false once the socket is
// closed (the signal to stop a receive loop).
bool receiveString(nng_socket socket, std::string& out)
{
    char* buffer = nullptr;
    auto size = std::size_t {0};

    auto result = nng_recv(socket, &buffer, &size, NNG_FLAG_ALLOC);
    if (result != 0)
        return false;

    out.assign(buffer, size);
    nng_free(buffer, size);
    return true;
}

} // namespace

// ---- RpcServer --------------------------------------------------------

RpcServer::RpcServer(Miro::Bridge& bridgeToUse, const std::string& url)
    : bridge(bridgeToUse)
{
    check(nng_rep0_open(&socket), "nng_rep0_open");
    check(nng_listen(socket, url.c_str(), nullptr, 0), "nng_listen");
    worker = std::thread([this] { receiveLoop(); });
}

RpcServer::~RpcServer()
{
    running = false;
    nng_close(socket); // unblocks nng_recv in the worker
    if (worker.joinable())
        worker.join();
}

void RpcServer::receiveLoop()
{
    // REP mandates strict recv -> send alternation, which this loop honours.
    while (running)
    {
        auto requestText = std::string {};
        if (!receiveString(socket, requestText))
            break;

        try
        {
            sendString(socket, dispatchEnvelope(bridge, requestText));
        }
        catch (const std::exception&)
        {
            break; // socket closed mid-send during shutdown
        }
    }
}

// ---- RpcClient --------------------------------------------------------

RpcClient::RpcClient(const std::string& url)
{
    check(nng_req0_open(&socket), "nng_req0_open");
    setTimeoutMs(5000);

    // Non-blocking dial: succeeds even if the Hub isn't up yet; the dialer
    // keeps retrying in the background and the first real call waits for it.
    check(nng_dial(socket, url.c_str(), nullptr, NNG_FLAG_NONBLOCK), "nng_dial");
}

RpcClient::~RpcClient()
{
    nng_close(socket);
}

void RpcClient::setTimeoutMs(int milliseconds)
{
    nng_socket_set_ms(socket, NNG_OPT_RECVTIMEO, milliseconds);
    nng_socket_set_ms(socket, NNG_OPT_SENDTIMEO, milliseconds);
}

Miro::JSON RpcClient::invokeRaw(const std::string& command,
                                const Miro::JSON& payload) const
{
    sendString(socket, Miro::Json::print(makeEnvelope(command, payload)));

    auto replyText = std::string {};
    if (!receiveString(socket, replyText))
        throw std::runtime_error("RPC '" + command + "' timed out or socket closed");

    auto reply = Miro::Json::parse(replyText);
    auto& object = reply.asObject();

    auto errorIt = object.find("error");
    if (errorIt != object.end() && errorIt->second.isString())
        throw std::runtime_error("RPC '" + command
                                 + "' failed: " + errorIt->second.asString());

    auto resultIt = object.find("result");
    return resultIt != object.end() ? resultIt->second : Miro::JSON {};
}

Invoke RpcClient::asInvoker() const
{
    return [this](const std::string& command, const Miro::JSON& payload)
    { return invokeRaw(command, payload); };
}

// ---- Publisher --------------------------------------------------------

Publisher::Publisher(Miro::Bridge& bridgeToUse, const std::string& url)
    : bridge(bridgeToUse)
    , emitListener(
          bridge.onEmit, [this] { onEmit(); }, EA::Listener::Modes::TriggerOnEvent)
{
    check(nng_pub0_open(&socket), "nng_pub0_open");
    check(nng_listen(socket, url.c_str(), nullptr, 0), "nng_listen");
}

Publisher::~Publisher()
{
    nng_close(socket);
}

void Publisher::onEmit()
{
    auto envelope = Miro::JSON {Miro::Json::Object {}};
    envelope.asObject()["event"] = Miro::JSON {std::string {bridge.currentEvent()}};
    envelope.asObject()["payload"] = bridge.currentPayload();

    auto text = Miro::Json::print(envelope);

    // PUB may be sent from either the server thread (remote submitPassword)
    // or the Hub's stdin thread (local password entry); serialise them.
    auto lock = std::scoped_lock {sendMutex};
    nng_send(socket, const_cast<char*>(text.data()), text.size(), 0);
}

// ---- Subscriber -------------------------------------------------------

Subscriber::Subscriber(const std::string& url, Handler handlerToUse)
    : handler(std::move(handlerToUse))
{
    check(nng_sub0_open(&socket), "nng_sub0_open");
    check(nng_socket_set(socket, NNG_OPT_SUB_SUBSCRIBE, "", 0),
          "nng_socket_set(subscribe)");
    check(nng_dial(socket, url.c_str(), nullptr, NNG_FLAG_NONBLOCK), "nng_dial");

    worker = std::thread([this] { receiveLoop(); });
}

Subscriber::~Subscriber()
{
    running = false;
    nng_close(socket);
    if (worker.joinable())
        worker.join();
}

void Subscriber::receiveLoop()
{
    while (running)
    {
        auto text = std::string {};
        if (!receiveString(socket, text))
            break;

        try
        {
            auto envelope = Miro::Json::parse(text);
            auto& object = envelope.asObject();
            auto event = object.at("event").asString();

            auto payloadIt = object.find("payload");
            auto payload =
                payloadIt != object.end() ? payloadIt->second : Miro::JSON {};

            handler(event, payload);
        }
        catch (const std::exception&)
        {
            // Ignore malformed frames; keep listening.
        }
    }
}

} // namespace hub::ipc
