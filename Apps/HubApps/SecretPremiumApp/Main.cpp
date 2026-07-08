#include "../Launcher.h"
#include "../Protocol.h"

#include "NngRpc.h"

#include <Miro/Miro.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace
{
using namespace std::chrono_literals;

std::string trim(std::string text)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), notSpace));
    text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(),
               text.end());
    return text;
}

// Ask the Hub for access, launching it (headless) if it isn't running.
hub::UnlockDecision requestAccess(hub::ipc::RpcClient& client, const char* argv0)
{
    auto request = hub::AppUnlockRequest {.appName = "SecretPremiumApp"};

    // Quick probe: already up?
    client.setTimeoutMs(800);
    try
    {
        return client.invoke<hub::UnlockDecision>("requestUnlock", request);
    }
    catch (const std::exception&)
    {
    }

    std::cout << "[App] The Hub isn't running — launching it headless.\n";
    auto exe = hub::siblingExecutable(argv0, "Hub", "Hub");
    if (!hub::launchDetached(exe, {"--headless"}))
        std::cout << "[App] Could not launch the Hub at " << exe << "\n";

    // The non-blocking dialer connects on its own once the Hub binds.
    client.setTimeoutMs(8000);
    for (auto attempt = 0; attempt < 20; ++attempt)
    {
        try
        {
            return client.invoke<hub::UnlockDecision>("requestUnlock", request);
        }
        catch (const std::exception&)
        {
            std::this_thread::sleep_for(300ms);
        }
    }

    throw std::runtime_error("Hub unreachable");
}

// Foreground password entry: this process owns the terminal, so it reads
// the secret and submits it to the Hub over IPC (Foo.h's "invoke the hub
// to open and enter the password").
bool unlockThroughHub(hub::ipc::RpcClient& client)
{
    client.setTimeoutMs(5000);
    auto line = std::string {};

    for (auto attempt = 0; attempt < 5; ++attempt)
    {
        std::cout << "[App] Enter the password to unlock the premium feature "
                     "(blank to give up): ";
        if (!std::getline(std::cin, line))
            return false;

        line = trim(line);
        if (line.empty())
            return false;

        auto decision = client.invoke<hub::UnlockDecision, hub::PasswordAttempt>(
            "submitPassword", {.password = line});

        if (decision.decision == hub::Decision::Unlocked)
            return true;

        std::cout << "[App] \033[1;31mHub rejected that password.\033[0m\n";
    }

    return false;
}

void revealFeature()
{
    // Little animated build-up, then the "dope-ass feature".
    std::cout << "\n[App] Access granted. Spinning up premium goodness";
    for (auto i = 0; i < 3; ++i)
    {
        std::cout << " ." << std::flush;
        std::this_thread::sleep_for(220ms);
    }
    std::cout << "\n\n";

    std::cout << "\033[1;35m"
              << "  ____  ____  ____  __  __  ___  __  __  __\n"
              << " (  _ \\(  _ \\( ___)(  \\/  )/ __)(  )(  )(  )\n"
              << "  )___/ )   / )__)  )    ( \\__ \\ )(__)(  )(__\n"
              << " (__)  (_)\\_)(____)(_/\\/\\_)(___/(______)(____)\n"
              << "\033[0m\n";

    std::cout << "\033[1;32m ✦ PREMIUM UNLOCKED ✦\033[0m   "
                 "welcome to the dope-ass feature\n\n";

    const char* perks[] = {
        "Quantum-smoothed gradients",
        "Buttery 120fps everything",
        "Infinite undo across timelines",
        "AI that actually reads your mind",
        "Zero-latency collaborative cursors",
    };

    for (auto* perk: perks)
    {
        std::cout << "   \033[1;36m➜\033[0m " << perk << "\n";
        std::this_thread::sleep_for(140ms);
    }

    std::cout << "\n   \033[2mThanks for entering 42.\033[0m\n\n";
}

} // namespace

int main(int argc, char** argv)
{
    (void) argc;
    auto client = hub::ipc::RpcClient {hub::rpcUrl};

    // Subscribe for async decision broadcasts — proves the pub/sub channel
    // and mirrors Foo.h's setOnUpdate (the app can react to an unlock that
    // happens anywhere in the suite, not just its own request).
    auto unlockedAsync = std::atomic<bool> {false};
    auto subscriber = hub::ipc::Subscriber {
        hub::eventUrl,
        [&](const std::string& event, const Miro::JSON& payload)
        {
            if (event != "decisionChanged")
                return;

            auto update = hub::UnlockUpdate {};
            Miro::fromJSON(update, payload);

            if (update.decision == hub::Decision::Unlocked)
            {
                unlockedAsync = true;
                std::cout << "\n[App] \033[2m(async) Hub broadcast: "
                          << update.message << "\033[0m\n";
            }
        }};

    std::cout << "[App] SecretPremiumApp starting. Asking the Hub for access...\n";

    try
    {
        auto decision = requestAccess(client, argv[0]);
        std::cout << "[App] Hub says: " << decision.message << "\n";

        if (decision.decision != hub::Decision::Unlocked)
        {
            if (!unlockThroughHub(client))
            {
                std::cout << "[App] Locked out. Bye.\n";
                return 1;
            }
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "[App] Could not reach the Hub: " << error.what() << "\n";
        return 1;
    }

    revealFeature();
    return 0;
}
