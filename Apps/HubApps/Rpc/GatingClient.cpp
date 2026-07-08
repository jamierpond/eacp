#include "GatingClient.h"

#include "Discovery.h"

#include <eacp/Network/HTTPRpc/RpcClient.h>

#include <algorithm>
#include <chrono>
#include <optional>

namespace hub::rpc
{

GatingClient::GatingClient(Options optionsToUse)
    : options(std::move(optionsToUse))
{
    worker = std::thread([this] { run(); });
}

GatingClient::~GatingClient()
{
    stopping = true;
    if (worker.joinable())
        worker.join();
}

void GatingClient::run()
{
    auto request = AppUnlockRequest {.appName = options.appName};
    auto interval =
        std::chrono::milliseconds {1000 / std::max(1, options.intervalHz)};

    auto launched = false;
    auto announced = false;
    auto last = std::optional<Decision> {};

    while (!stopping)
    {
        auto reached = false;

        if (auto url = readEndpoint(options.hubName))
        {
            try
            {
                auto client = eacp::HTTP::Rpc::Client {*url};

                // Announce once (so the Hub UI can show "app wants access"),
                // then just poll the current decision.
                auto decision = announced
                    ? client.invoke<UnlockDecision>("getDecision")
                    : client.invoke<UnlockDecision>("requestUnlock", request);
                announced = true;
                reached = true;

                if (!last || *last != decision.decision)
                {
                    last = decision.decision;
                    options.onDecision(decision);
                }

                if (decision.decision == Decision::Unlocked)
                {
                    options.onUnlock();
                    return; // nothing more to poll for
                }
            }
            catch (const std::exception&)
            {
                // Hub down or endpoint stale — fall through to relaunch.
            }
        }

        if (!reached && !launched)
        {
            options.launchHub();
            launched = true;
        }

        std::this_thread::sleep_for(interval);
    }
}

} // namespace hub::rpc
