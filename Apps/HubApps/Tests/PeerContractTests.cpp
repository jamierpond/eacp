// The two-way contract, exercised entirely in one process: the same Peer
// class serves an API and calls other peers, so a Hub peer and an app peer
// talk over the loopback exactly as the two apps do — and a peer can even
// call itself. The event loop is pumped while the client work runs on a
// worker thread (the HTTP server accepts on the main run loop).

#include "../GatingApi.h"

#include "Peer.h"

#include <eacp/Core/Threads/EventLoop.h>

#include <NanoTest/NanoTest.h>

#include <atomic>
#include <chrono>
#include <thread>

using namespace nano;
using namespace std::chrono_literals;
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

auto tPeersTalkBothDirections = test("HubApps/peersTalkBothDirections") = []
{
    // Two peers in one process — Hub side and app side.
    auto hubPeer = hub::rpc::Peer {0};
    auto appPeer = hub::rpc::Peer {0};

    auto gating = hub::GatingApi {};
    hubPeer.serve(gating);

    auto app = hub::ClientApi {};
    auto gotUnlock = std::atomic<bool> {false};
    app.onUpdate = [&](const hub::UnlockUpdate& update)
    {
        if (update.decision == hub::Decision::Unlocked)
            gotUnlock = true;
    };
    appPeer.serve(app);

    auto unlockedReply = false;
    auto callbackOk = false;

    auto stopped = withLoop(
        [&]
        {
            // app -> hub: the app peer is the client, the hub peer the server.
            appPeer.call<hub::Ack>(
                hubPeer.baseUrl(),
                "subscribe",
                hub::SubscribeRequest {.appName = "test",
                                       .callbackUrl = appPeer.baseUrl()});

            auto decision = appPeer.call<hub::UnlockDecision>(
                hubPeer.baseUrl(),
                "submitPassword",
                hub::PasswordAttempt {.password = "42"});
            unlockedReply = decision.decision == hub::Decision::Unlocked;

            // hub -> app: roles reversed, same Peer class on both sides.
            auto ack = hubPeer.call<hub::Ack>(
                appPeer.baseUrl(),
                "notifyDecision",
                hub::UnlockUpdate {.decision = hub::Decision::Unlocked,
                                   .message = "unlocked"});
            callbackOk = ack.ok;
        });

    check(stopped);
    check(unlockedReply);
    check(callbackOk);
    check(gotUnlock.load());
};

auto tWrongPasswordStaysLocked = test("HubApps/wrongPasswordStaysLocked") = []
{
    // A peer calling itself: one Peer serves the GatingApi and drives it.
    auto peer = hub::rpc::Peer {0};
    auto gating = hub::GatingApi {};
    peer.serve(gating);

    auto stayedLocked = false;

    auto stopped = withLoop(
        [&]
        {
            auto decision = peer.call<hub::UnlockDecision>(
                peer.baseUrl(),
                "submitPassword",
                hub::PasswordAttempt {.password = "not-42"});
            stayedLocked = decision.decision != hub::Decision::Unlocked;
        });

    check(stopped);
    check(stayedLocked);
};
