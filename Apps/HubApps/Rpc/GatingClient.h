#pragma once

// A stock polling client for the gating service — the client half of the
// demo, with no server of its own.
//
// It discovers the Hub via its rendezvous endpoint (launching it if a
// launcher is supplied), announces the app once, then polls getDecision on
// a background thread at a fixed interval. Every decision change fires
// onDecision; the transition to Unlocked additionally fires onUnlock (the
// common case), after which polling stops. Callbacks run on the poll
// thread — marshal to your UI thread inside them.

#include "../GatingApi.h"

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace hub::rpc
{

class GatingClient
{
public:
    struct Options
    {
        std::string appName = "app";
        std::string hubName = "hub"; // rendezvous name to discover

        int intervalHz = 5;          // ~200 ms
        std::function<void()> launchHub = [] {};
        std::function<void(const UnlockDecision&)> onDecision =
            [](const UnlockDecision&) {};
        std::function<void()> onUnlock = [] {};
    };

    explicit GatingClient(Options options);
    ~GatingClient();

    GatingClient(const GatingClient&) = delete;
    GatingClient& operator=(const GatingClient&) = delete;

private:
    void run();

    Options options;
    std::atomic<bool> stopping {false};
    std::thread worker;
};

} // namespace hub::rpc
