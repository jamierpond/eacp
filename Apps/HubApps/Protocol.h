#pragma once

// Shared, statically-typed protocol for the Hub <-> SecretPremiumApp demo.
//
// Both apps include this header. The Hub binds `GatingApi` to a
// Miro::Bridge and serves it; the app drives the same command names
// through a typed nng client (see Ipc/NngRpc.h). Miro handles all
// JSON (de)serialisation from the reflected structs below — nng only
// moves the bytes.

#include <Miro/Miro.h>

#include <mutex>
#include <string>

namespace hub
{

// Fixed transport endpoints. TCP loopback keeps the demo identical on
// macOS and Windows (nng ipc:// paths differ per-platform).
inline constexpr auto rpcUrl = "tcp://127.0.0.1:8121"; // request/reply
inline constexpr auto eventUrl = "tcp://127.0.0.1:8122"; // pub/sub
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

// ---- Wire types (request / reply / event payloads) --------------------

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

// Payload of the `decisionChanged` pub/sub event — the async update an
// app receives while a human is (maybe) typing a password at the Hub.
struct UnlockUpdate
{
    Decision decision = Decision::Standby;
    std::string message;
    std::string requestedByApp;

    MIRO_REFLECT(decision, message, requestedByApp)
};

// ---- The gated API the Hub serves over IPC ----------------------------
//
// One instance lives in the Hub. Its methods run on the nng server
// thread (remote calls) and the mutable state is also read/written by
// the Hub's interactive stdin thread, so everything funnels through a
// mutex. `decisionChanged` is the only outgoing channel — publishing it
// fans out to every subscribed app.
class GatingApi
{
public:
    void reflect(Miro::ApiReflector& reflector)
    {
        reflector.command(&GatingApi::requestUnlock, "requestUnlock");
        reflector.command(&GatingApi::getDecision, "getDecision");
        reflector.command(&GatingApi::submitPassword, "submitPassword");
        reflector.event(&GatingApi::decisionChanged, "decisionChanged");
    }

    // An app asks whether it may run. Already unlocked -> say yes;
    // otherwise flag LOGIN_REQUIRED so the Hub operator knows a password
    // is expected, and report that back synchronously. The real unlock
    // arrives later, asynchronously, via decisionChanged.
    UnlockDecision requestUnlock(const AppUnlockRequest& request)
    {
        auto lock = std::scoped_lock {stateMutex};
        requestedApp = request.appName;

        if (current != Decision::Unlocked)
            current = Decision::LoginRequired;

        return snapshotLocked();
    }

    UnlockDecision getDecision() const
    {
        auto lock = std::scoped_lock {stateMutex};
        return snapshotLocked();
    }

    // Password entry — usable locally by the Hub operator or remotely by
    // an app that already knows the secret. Flips the shared decision and
    // broadcasts the result to all subscribers.
    UnlockDecision submitPassword(const PasswordAttempt& attempt)
    {
        auto update = UnlockUpdate {};
        {
            auto lock = std::scoped_lock {stateMutex};
            current = attempt.password == secretPassword ? Decision::Unlocked
                                                         : Decision::LoginRequired;
            update = {.decision = current,
                      .message = describe(current),
                      .requestedByApp = requestedApp};
        }

        decisionChanged.publish(update);
        return {.decision = update.decision, .message = update.message};
    }

    bool isUnlocked() const
    {
        auto lock = std::scoped_lock {stateMutex};
        return current == Decision::Unlocked;
    }

    std::string pendingApp() const
    {
        auto lock = std::scoped_lock {stateMutex};
        return current == Decision::Unlocked ? std::string {} : requestedApp;
    }

    Miro::Event<UnlockUpdate> decisionChanged;

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
