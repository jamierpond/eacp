// Tests for Ipc::Peer — an RPC server that also acts as a client, exercised
// entirely in one process. The event loop is pumped while the client work runs
// on a worker thread (the HTTP server accepts on the main run loop).

#include <eacp/InterAppCommunication/Peer.h>

#include <eacp/Core/Threads/EventLoop.h>

#include <Miro/Miro.h>

#include <NanoTest/NanoTest.h>

#include <chrono>
#include <string>
#include <thread>

using namespace nano;
using eacp::Threads::callAsync;
using eacp::Threads::stopEventLoop;
namespace Ipc = eacp::Ipc;

namespace
{

struct AddRequest
{
    int a = 0;
    int b = 0;
    MIRO_REFLECT(a, b)
};

struct AddResponse
{
    int sum = 0;
    MIRO_REFLECT(sum)
};

struct Pong
{
    std::string message;
    MIRO_REFLECT(message)
};

class MathApi
{
public:
    MIRO_REFLECT_API(add, ping)

    AddResponse add(const AddRequest& request)
    {
        return {.sum = request.a + request.b};
    }

    Pong ping() { return {.message = "pong"}; }
};

// Pump the loop while `clientWork` runs on a worker thread, then stop.
template <typename Fn>
bool withLoop(Fn&& clientWork)
{
    auto worker = std::thread();
    auto stopped = eacp::Threads::runEventLoopFor(
        std::chrono::seconds(5),
        [&]
        {
            worker = std::thread(
                [&]
                {
                    clientWork();
                    callAsync([] { stopEventLoop(); });
                });
        });
    worker.join();
    return stopped;
}

} // namespace

auto tBindsToFreePort = test("Ipc.Peer/bindsToFreePort") = []
{
    auto peer = Ipc::Peer {0};

    check(peer.boundPort() > 0);
    check(peer.baseUrl()
          == "http://127.0.0.1:" + std::to_string(peer.boundPort()) + "/rpc");
};

auto tServesAndCallsItselfWithRequest =
    test("Ipc.Peer/servesAndCallsItselfWithRequest") = []
{
    auto peer = Ipc::Peer {0};
    auto api = MathApi {};
    peer.serve(api);

    auto sum = 0;
    auto stopped = withLoop(
        [&]
        {
            sum = peer.call<AddResponse>(
                          peer.baseUrl(), "add", AddRequest {.a = 2, .b = 40})
                      .sum;
        });

    check(stopped);
    check(sum == 42);
};

auto tCallWithoutRequest = test("Ipc.Peer/callWithoutRequest") = []
{
    auto peer = Ipc::Peer {0};
    auto api = MathApi {};
    peer.serve(api);

    auto message = std::string {};
    auto stopped =
        withLoop([&] { message = peer.call<Pong>(peer.baseUrl(), "ping").message; });

    check(stopped);
    check(message == "pong");
};

auto tTwoPeersDistinctPortsOneCallsTheOther =
    test("Ipc.Peer/twoPeersDistinctPortsOneCallsTheOther") = []
{
    auto server = Ipc::Peer {0};
    auto clientPeer = Ipc::Peer {0};

    // Two peers in one process land on different OS-assigned ports.
    check(server.boundPort() != clientPeer.boundPort());

    auto api = MathApi {};
    server.serve(api);

    auto sum = 0;
    auto stopped = withLoop(
        [&]
        {
            sum = clientPeer
                      .call<AddResponse>(
                          server.baseUrl(), "add", AddRequest {.a = 1, .b = 2})
                      .sum;
        });

    check(stopped);
    check(sum == 3);
};

auto tUnknownCommandThrows = test("Ipc.Peer/unknownCommandThrows") = []
{
    auto peer = Ipc::Peer {0};
    auto api = MathApi {};
    peer.serve(api);

    auto threw = false;
    auto stopped = withLoop(
        [&]
        {
            try
            {
                (void) peer.call<Pong>(peer.baseUrl(), "nope");
            }
            catch (const std::exception&)
            {
                threw = true;
            }
        });

    check(stopped);
    check(threw);
};
