#pragma once

// Shared, statically-typed contract for the Hub <-> SecretPremiumApp demo.
//
// The Hub is the only server: it mounts this GatingApi on an eacp::Ipc::Peer
// (eacp/InterAppCommunication/Peer.h). Apps are pure clients — they poll
// getDecision via an Ipc::PollingClient (PollingClient.h), so nothing runs a
// server on the client side. Miro does all JSON (de)serialisation from the
// reflected structs below.

#include "Miro/Reflection/ReflectMacro.h"
#include <Miro/Miro.h>

#include <functional>
#include <mutex>
#include <string>

namespace hub
{

inline constexpr auto secretPassword = "42";

// Mirrors Foo.h's Decision. Miro reflects `enum class` automatically and
// serialises each value as its enumerator name.
enum class Decision
{
    Standby, // hub up, suite locked, nobody has asked yet
    LoginRequired, // an app asked to unlock; a password is expected
    Unlocked, // password accepted — apps may reveal their features
    MustUpdate, // version mismatch — update the hub / app suite
    UnknownError, // damaged install — reinstall the hub / app suite
};

inline std::string describe(Decision decision)
{
    switch (decision)
    {
        case Decision::Standby:
            return "Suite locked";
        case Decision::LoginRequired:
            return "Enter the password at the Hub";
        case Decision::Unlocked:
            return "Premium suite unlocked";
        case Decision::MustUpdate:
            return "A required update is available";
        case Decision::UnknownError:
            return "Installation damaged — reinstall the Hub";
    }
    return {};
}

// ---- Wire types -------------------------------------------------------

struct AppUnlockRequest
{
    std::string appName;
    int schemaVersion = 1;

    MIRO_REFLECT(appName, schemaVersion)
};

struct PasswordAttempt
{
    std::string password;

    MIRO_REFLECT(password)
};

struct UnlockDecision
{
    Decision decision = Decision::Standby;
    std::string message;

    MIRO_REFLECT(decision, message)
};

// ---- Hub-side API -----------------------------------------------------
//
// One instance lives in the Hub. Handlers run on the HTTP dispatcher; the
// state is also touched by the Hub UI, so it funnels through a mutex.
class GatingApi
{
public:
    MIRO_REFLECT_API(requestUnlock, getDecision, submitPassword)

    // An app announces itself. Already unlocked -> yes; otherwise flag
    // LOGIN_REQUIRED so the Hub operator sees a password is expected. The
    // app then polls getDecision for the real unlock.
    UnlockDecision requestUnlock(const AppUnlockRequest& request)
    {
        auto snapshot = UnlockDecision {};
        {
            auto lock = std::scoped_lock {stateMutex};
            requestedApp = request.appName;

            if (current != Decision::Unlocked)
                current = Decision::LoginRequired;

            snapshot = snapshotLocked();
        }

        onAccessRequested(request.appName);
        return snapshot;
    }

    UnlockDecision getDecision() const
    {
        auto lock = std::scoped_lock {stateMutex};
        return snapshotLocked();
    }

    // Password entry (from the Hub UI, or a client). Flips the shared
    // decision; polling apps pick it up on their next tick.
    UnlockDecision submitPassword(const PasswordAttempt& attempt)
    {
        auto lock = std::scoped_lock {stateMutex};
        current = attempt.password == secretPassword ? Decision::Unlocked
                                                     : Decision::LoginRequired;
        return snapshotLocked();
    }

    bool isUnlocked() const
    {
        auto lock = std::scoped_lock {stateMutex};
        return current == Decision::Unlocked;
    }

    // Fired (on the HTTP dispatcher) when an app announces itself; the Hub
    // marshals it to the UI. Non-null default so call sites never check.
    std::function<void(const std::string& appName)> onAccessRequested =
        [](const std::string&) {};

private:
    UnlockDecision snapshotLocked() const
    {
        return {.decision = current, .message = describe(current)};
    }

    mutable std::mutex stateMutex;
    Decision current = Decision::Standby;
    std::string requestedApp;
};

} // namespace hub
