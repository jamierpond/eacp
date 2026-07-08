#include "../Launcher.h"
#include "../Protocol.h"

#include "NngRpc.h"

#include <eacp/Core/Threads/EventLoop.h>

#include <Miro/Miro.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{

std::string trim(std::string text)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), notSpace));
    text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(),
               text.end());
    return text;
}

void printBanner()
{
    std::cout
        << "\n"
        << "\033[1;36m"
        << "  ______  ______  ______  ______   __  __  __  __  ______\n"
        << " /\\  ___\\/\\  __ \\/\\  ___\\/\\  == \\ /\\ \\_\\ \\/\\ \\/\\ \\/\\  == \\\n"
        << " \\ \\  __\\\\ \\  __ \\ \\ \\___\\ \\  _-/ \\ \\  __ \\ \\ \\_\\ \\ \\  __<\n"
        << "  \\ \\_____\\ \\_\\ \\_\\ \\_____\\ \\_\\    \\ \\_\\ \\_\\ \\_____\\ \\_____\\\n"
        << "   \\/_____/\\/_/\\/_/\\/_____/\\/_/     \\/_/\\/_/\\/_____/\\/_____/\n"
        << "\033[0m"
        << "        product gating hub  ·  the answer is 42\n\n";
}

int runHeadless(hub::GatingApi& api)
{
    // A headless Hub is a detached daemon: release the launcher's terminal
    // so we don't hold its stdout pipe open or fight it for stdin. Keep a
    // log so the session is still inspectable.
    auto logPath = std::filesystem::temp_directory_path() / "eacp-hub.log";
    std::freopen(logPath.string().c_str(), "w", stdout);
    std::freopen(logPath.string().c_str(), "a", stderr);
#ifdef _WIN32
    std::freopen("NUL", "r", stdin);
#else
    std::freopen("/dev/null", "r", stdin);
#endif

    std::cout << "[Hub] Running headless. Serving IPC on " << hub::rpcUrl << " / "
              << hub::eventUrl << "; waiting for a password.\n";
    std::cout.flush();
    (void) api;
    eacp::Threads::runEventLoop();
    return 0;
}

int runInteractive(hub::GatingApi& api, const char* argv0)
{
    printBanner();
    std::cout << "[Hub] Product gating server online.\n"
              << "[Hub]   rpc:    " << hub::rpcUrl << "\n"
              << "[Hub]   events: " << hub::eventUrl << "\n\n";

    auto line = std::string {};
    while (true)
    {
        if (api.isUnlocked())
        {
            std::cout << "\n[Hub] Suite is UNLOCKED.  [l] launch premium app   "
                         "[q] quit\n> ";
            if (!std::getline(std::cin, line))
                break;

            line = trim(line);
            if (line == "q" || line == "quit")
                break;

            if (line == "l" || line == "launch")
            {
                auto exe = hub::siblingExecutable(
                    argv0, "SecretPremiumApp", "SecretPremiumApp");
                std::cout << "[Hub] Launching " << exe << " ...\n";
                if (!hub::launchDetached(exe))
                    std::cout << "[Hub] Could not launch it — is it built?\n";
            }
            continue;
        }

        auto pending = api.pendingApp();
        if (!pending.empty())
            std::cout << "[Hub] '" << pending << "' is knocking and needs access.\n";

        std::cout << "[Hub] Enter the password (hint: the answer to life, the "
                     "universe, and everything): ";
        if (!std::getline(std::cin, line))
            break;

        auto decision = api.submitPassword({.password = trim(line)});
        if (decision.decision == hub::Decision::Unlocked)
            std::cout << "[Hub] \033[1;32mCorrect!\033[0m Suite unlocked — "
                         "subscribers notified.\n";
        else
            std::cout << "[Hub] \033[1;31mWrong password.\033[0m Try again.\n";
    }

    std::cout << "[Hub] Shutting down.\n";
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    auto headless = false;
    for (auto i = 1; i < argc; ++i)
        if (std::string {argv[i]} == "--headless")
            headless = true;

    // Lifetime: api first (destructed last), then bridge, then the
    // transports that reference the bridge.
    auto api = hub::GatingApi {};
    auto bridge = Miro::Bridge {};
    bridge.use(api);

    auto server = hub::ipc::RpcServer {bridge, hub::rpcUrl};
    auto publisher = hub::ipc::Publisher {bridge, hub::eventUrl};

    return headless ? runHeadless(api) : runInteractive(api, argv[0]);
}
