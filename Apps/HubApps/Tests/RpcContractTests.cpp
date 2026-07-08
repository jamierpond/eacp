// The RPC contract, exercised entirely in one process. An Ipc::Peer serves the
// API and can also call peers — so it talks to itself, and a second peer (the
// app side) talks to it, exactly as the two apps do over the loopback. The
// event loop is pumped while the client work runs on a worker thread (the HTTP
// server accepts on the main run loop).

#include "../GatingApi.h"

#include <eacp/InterAppCommunication/Peer.h>

#include <eacp/Core/Threads/EventLoop.h>

#include <NanoTest/NanoTest.h>

#include <thread>

using namespace nano;
using eacp::Threads::callAsync;
using eacp::Threads::stopEventLoop;

namespace
{
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

auto tServerTalksToItself = test("HubApps/serverTalksToItself") = []
{
    auto server = eacp::Ipc::Peer {0};
    auto gating = hub::GatingApi {};
    server.serve(gating);

    auto lockedBefore = false;
    auto lockedOnWrong = false;
    auto unlockedOn42 = false;

    auto stopped = withLoop(
        [&]
        {
            auto url = server.baseUrl();

            lockedBefore =
                server.call<hub::UnlockDecision>(url, "getDecision").decision
                != hub::Decision::Unlocked;

            lockedOnWrong = server
                                .call<hub::UnlockDecision>(
                                    url,
                                    "submitPassword",
                                    hub::PasswordAttempt {.password = "nope"})
                                .decision
                            != hub::Decision::Unlocked;

            server.call<hub::UnlockDecision>(
                url, "submitPassword", hub::PasswordAttempt {.password = "42"});

            unlockedOn42 =
                server.call<hub::UnlockDecision>(url, "getDecision").decision
                == hub::Decision::Unlocked;
        });

    check(stopped);
    check(lockedBefore);
    check(lockedOnWrong);
    check(unlockedOn42);
};

auto tTwoServersOneProcess = test("HubApps/twoServersOneProcess") = []
{
    // Two peers in one process — Hub side and app side. The app side acts
    // purely as a client here.
    auto hubServer = eacp::Ipc::Peer {0};
    auto appServer = eacp::Ipc::Peer {0};

    auto gating = hub::GatingApi {};
    hubServer.serve(gating);

    auto unlocked = false;

    auto stopped = withLoop(
        [&]
        {
            appServer.call<hub::UnlockDecision>(
                hubServer.baseUrl(),
                "submitPassword",
                hub::PasswordAttempt {.password = "42"});

            unlocked =
                appServer
                    .call<hub::UnlockDecision>(hubServer.baseUrl(), "getDecision")
                    .decision
                == hub::Decision::Unlocked;
        });

    check(stopped);
    check(unlocked);
};
