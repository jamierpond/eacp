#pragma once

// A generic background polling client for an Ipc::Peer.
//
// It discovers a peer via its rendezvous endpoint (launching it if a launcher
// is supplied), then polls it on a background thread at a fixed interval. The
// protocol is entirely yours: `poll` performs one request against a connected
// client and returns whatever response type you like. `onChange` fires when a
// response differs from the previous one (per `changed`), and `finished` lets
// you stop polling once a terminal state is reached (firing `onFinish` first).
//
// All callbacks run on the poll thread — marshal to your UI thread inside them.
//
// Example — poll a decision until it flips to Unlocked:
//
//   auto options = PollingClient<UnlockDecision>::Options {};
//   options.endpointName = "hub";
//   options.poll = [](auto& client)
//   { return client.template invoke<UnlockDecision>("getDecision"); };
//   options.changed = [](auto& a, auto& b) { return a.state != b.state; };
//   options.onChange = [](auto& d) { showStatus(d); };
//   options.finished = [](auto& d) { return d.state == State::Unlocked; };
//   options.onFinish = [](auto&) { reveal(); };
//   auto client = PollingClient<UnlockDecision> {std::move(options)};

#include "Endpoint.h"

#include <eacp/Network/HTTPRpc/RpcClient.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <thread>

namespace eacp::Ipc
{

template <typename Response>
class PollingClient
{
public:
    using RpcClient = eacp::HTTP::Rpc::Client;

    struct Options
    {
        // Rendezvous name of the peer to discover (see Endpoint.h).
        std::string endpointName = "hub";

        // Poll rate. 5 Hz ~= every 200 ms.
        int intervalHz = 5;

        // Perform one request against a connected peer and return its current
        // state. Runs on the poll thread every tick. Required — supply your
        // own command(s); capture per-client state (e.g. an "announced once"
        // flag) in the closure to vary the request across ticks.
        std::function<Response(RpcClient&)> poll;

        // Fired when a successful poll differs from the previous one, as
        // decided by `changed` (default: every poll counts as a change).
        std::function<bool(const Response& previous, const Response& latest)>
            changed = [](const Response&, const Response&) { return true; };
        std::function<void(const Response&)> onChange = [](const Response&) {};

        // When `finished` returns true, `onFinish` fires once and polling
        // stops. Default: never finish, poll for the client's lifetime.
        std::function<bool(const Response&)> finished = [](const Response&)
        { return false; };
        std::function<void(const Response&)> onFinish = [](const Response&) {};

        // Called once if the peer's endpoint can't be reached, to start it.
        std::function<void()> launch = [] {};
    };

    explicit PollingClient(Options options)
        : options(std::move(options))
    {
        worker = std::thread([this] { run(); });
    }

    ~PollingClient()
    {
        stopping = true;
        if (worker.joinable())
            worker.join();
    }

    PollingClient(const PollingClient&) = delete;
    PollingClient& operator=(const PollingClient&) = delete;

private:
    void run()
    {
        auto interval =
            std::chrono::milliseconds {1000 / std::max(1, options.intervalHz)};

        auto launched = false;
        auto last = std::optional<Response> {};

        while (!stopping)
        {
            auto reached = false;

            if (auto url = readEndpoint(options.endpointName))
            {
                try
                {
                    auto client = RpcClient {*url};
                    auto response = options.poll(client);
                    reached = true;

                    if (!last || options.changed(*last, response))
                    {
                        last = response;
                        options.onChange(response);
                    }

                    if (options.finished(response))
                    {
                        options.onFinish(response);
                        return; // terminal state — nothing more to poll for
                    }
                }
                catch (const std::exception&)
                {
                    // Peer down or endpoint stale — fall through to relaunch.
                }
            }

            if (!reached && !launched)
            {
                options.launch();
                launched = true;
            }

            std::this_thread::sleep_for(interval);
        }
    }

    Options options;
    std::atomic<bool> stopping {false};
    std::thread worker;
};

} // namespace eacp::Ipc
