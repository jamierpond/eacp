// Tests for the generic Ipc::PollingClient. The peer-backed cases construct a
// real Peer, advertise it via an endpoint, and let the client's own worker
// thread poll it while the main thread pumps the loop; a callback stops the
// loop once the client reaches its terminal state.

#include <eacp/InterAppCommunication/PollingClient.h>

#include <eacp/InterAppCommunication/Endpoint.h>
#include <eacp/InterAppCommunication/Peer.h>

#include <eacp/Core/Threads/EventLoop.h>

#include <Miro/Miro.h>

#include <NanoTest/NanoTest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <string>

using namespace nano;
using eacp::Threads::callAsync;
using eacp::Threads::stopEventLoop;
namespace Ipc = eacp::Ipc;

namespace
{

struct Tick
{
    int value = 0;
    MIRO_REFLECT(value)
};

// "next" returns a strictly increasing value — the client can poll until it
// crosses a threshold.
class CounterApi
{
public:
    MIRO_REFLECT_API(next)

    Tick next() { return {.value = ++counter}; }

private:
    std::atomic<int> counter {0};
};

// "next" always returns the same value — so a value-based `changed` predicate
// only ever sees one change.
class ConstantApi
{
public:
    MIRO_REFLECT_API(next)

    Tick next() { return {.value = 7}; }
};

using Poller = Ipc::PollingClient<Tick>;

} // namespace

auto tPollsUntilTerminalAndFiresCallbacks =
    test("Ipc.PollingClient/pollsUntilTerminalAndFiresCallbacks") = []
{
    auto name = std::string {"iac-test-poll-terminal"};
    Ipc::removeEndpoint(name);

    auto peer = Ipc::Peer {0};
    auto api = CounterApi {};
    peer.serve(api);
    Ipc::writeEndpoint(name, peer.baseUrl());

    auto changeCount = std::atomic<int> {0};
    auto finishCount = std::atomic<int> {0};
    auto lastValue = std::atomic<int> {0};

    auto options = Poller::Options {};
    options.endpointName = name;
    options.intervalHz = 50;
    options.poll = [](Poller::RpcClient& client)
    { return client.invoke<Tick>("next"); };
    options.onChange = [&](const Tick& tick)
    {
        lastValue = tick.value;
        changeCount.fetch_add(1);
    };
    options.finished = [](const Tick& tick) { return tick.value >= 3; };
    options.onFinish = [&](const Tick&)
    {
        finishCount.fetch_add(1);
        callAsync([] { stopEventLoop(); });
    };

    auto client = Poller {std::move(options)};

    auto stopped = eacp::Threads::runEventLoopFor(std::chrono::seconds(5));

    Ipc::removeEndpoint(name);

    check(stopped);
    check(finishCount.load() == 1); // onFinish fires exactly once
    check(lastValue.load() >= 3); // terminal value was surfaced
    check(changeCount.load() >= 1); // at least the first poll counted as change
};

auto tLaunchesWhenPeerUnreachable =
    test("Ipc.PollingClient/launchesWhenPeerUnreachable") = []
{
    auto name = std::string {"iac-test-poll-launch"};
    Ipc::removeEndpoint(name); // no endpoint advertised -> unreachable

    auto launchCount = std::atomic<int> {0};
    auto launched = std::promise<void> {};

    auto options = Poller::Options {};
    options.endpointName = name;
    options.intervalHz = 50;
    options.launch = [&]
    {
        if (launchCount.fetch_add(1) == 0)
            launched.set_value();
    };
    // Never invoked while unreachable, but must be a valid callable.
    options.poll = [](Poller::RpcClient&) { return Tick {}; };

    auto client = Poller {std::move(options)};

    auto status = launched.get_future().wait_for(std::chrono::seconds(2));

    check(status == std::future_status::ready); // launch ran
    check(launchCount.load() == 1); // and only once (guarded)
};

auto tOnChangeFiresOnlyWhenChanged =
    test("Ipc.PollingClient/onChangeFiresOnlyWhenChanged") = []
{
    auto name = std::string {"iac-test-poll-changed"};
    Ipc::removeEndpoint(name);

    auto peer = Ipc::Peer {0};
    auto api = ConstantApi {}; // always returns value 7
    peer.serve(api);
    Ipc::writeEndpoint(name, peer.baseUrl());

    auto changeCount = std::atomic<int> {0};
    auto pollCount = std::atomic<int> {0};

    auto options = Poller::Options {};
    options.endpointName = name;
    options.intervalHz = 100;
    options.poll = [&](Poller::RpcClient& client)
    {
        pollCount.fetch_add(1);
        return client.invoke<Tick>("next");
    };
    options.changed = [](const Tick& previous, const Tick& latest)
    { return previous.value != latest.value; };
    options.onChange = [&](const Tick&) { changeCount.fetch_add(1); };
    options.finished = [&](const Tick&) { return pollCount.load() >= 5; };
    options.onFinish = [&](const Tick&) { callAsync([] { stopEventLoop(); }); };

    auto client = Poller {std::move(options)};

    auto stopped = eacp::Threads::runEventLoopFor(std::chrono::seconds(5));

    Ipc::removeEndpoint(name);

    check(stopped);
    check(pollCount.load() >= 5);
    // The value never changes, so onChange fires only for the first poll.
    check(changeCount.load() == 1);
};
