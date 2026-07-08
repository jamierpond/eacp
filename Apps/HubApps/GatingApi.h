#pragma once

// Shared, statically-typed contract for the Hub <-> SecretPremiumApp demo.
//
// Transport is local HTTP (see Rpc/Peer.h): each side runs a Miro::Bridge
// mounted on an eacp::HTTP server AND acts as a client to the other — a
// two-way arrangement, so the Hub can push a decision to the app the same
// way the app calls the Hub. Miro does all JSON (de)serialisation from the
// reflected structs below; the transport just moves bytes.
//
// These API classes are transport-agnostic: they expose std::function
// hooks (onAccessRequested / onDecisionChanged / onUpdate) that the app
// wires to HTTP callbacks and UI updates. That keeps this header free of
// any HTTP dependency and lets the same classes be driven in a unit test.

#include <Miro/Miro.h>

#include <algorithm>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace hub
{

inline constexpr auto secretPassword = "42";

// Mirrors Foo.h's Decision. Miro reflects `enum class` automatically and
// serialises each value as its enumerator name.
enum class Decision
{
    Standby,       // hub up, suite locked, nobody has asked yet
    LoginRequired, // an app asked to unlock; a password is expected
    Unlocked,      // password accepted — apps may reveal their features
    MustUpdate,    // version mismatch — update the hub / app suite
    UnknownError,  // damaged install — reinstall the hub / app suite
};

inline std::string describe(Decision decision)
{
    switch (decision)
    {
        case Decision::Standby: return "Suite locked";
        case Decision::LoginRequired: return "Enter the password at the Hub";
        case Decision::Unlocked: return "Premium suite unlocked";
        case Decision::MustUpdate: return "A required update is available";
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

// An app registering the base URL of its own peer so the Hub can call it
// back when the decision changes (the two-way half of the protocol).
struct SubscribeRequest
{
    std::string appName;
    std::string callbackUrl;

    MIRO_REFLECT(appName, callbackUrl)
};

struct Ack
{
    bool ok = true;

    MIRO_REFLECT(ok)
};

// Payload the Hub pushes to each subscribed app when the decision changes.
struct UnlockUpdate
{
    Decision decision = Decision::Standby;
    std::string message;

    MIRO_REFLECT(decision, message)
};

// ---- Hub-side API -----------------------------------------------------
//
// One instance lives in the Hub. Handlers run on the HTTP dispatcher; the
// mutable state is also touched by the Hub UI, so everything funnels
// through a mutex. The onDecisionChanged hook hands the Hub the update and
// the current subscriber list to fan out over HTTP.
class GatingApi
{
public:
    void reflect(Miro::ApiReflector& reflector)
    {
        reflector.command(&GatingApi::requestUnlock, "requestUnlock");
        reflector.command(&GatingApi::getDecision, "getDecision");
        reflector.command(&GatingApi::submitPassword, "submitPassword");
        reflector.command(&GatingApi::subscribe, "subscribe");
        reflector.command(&GatingApi::focus, "focus");
    }

    // Single-instance: a second launch calls this to raise the running
    // window instead of opening its own.
    Ack focus()
    {
        onFocus();
        return {};
    }

    // An app asks whether it may run. Already unlocked -> yes; otherwise
    // flag LOGIN_REQUIRED so the Hub operator sees a password is expected.
    // The real unlock is delivered later via the callback.
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

    // An app registers a callback URL. Idempotent.
    Ack subscribe(const SubscribeRequest& request)
    {
        auto lock = std::scoped_lock {stateMutex};
        if (std::find(subscribers.begin(),
                      subscribers.end(),
                      request.callbackUrl)
            == subscribers.end())
            subscribers.push_back(request.callbackUrl);
        return {};
    }

    // Password entry (from the Hub UI, or remotely). Flips the shared
    // decision and hands the resulting update + subscriber list to the
    // onDecisionChanged hook, which fans it out over HTTP.
    UnlockDecision submitPassword(const PasswordAttempt& attempt)
    {
        auto update = UnlockUpdate {};
        auto targets = std::vector<std::string> {};
        {
            auto lock = std::scoped_lock {stateMutex};
            current = attempt.password == secretPassword ? Decision::Unlocked
                                                         : Decision::LoginRequired;
            update = {.decision = current, .message = describe(current)};
            targets = subscribers;
        }

        onDecisionChanged(update, targets);
        return {.decision = update.decision, .message = update.message};
    }

    bool isUnlocked() const
    {
        auto lock = std::scoped_lock {stateMutex};
        return current == Decision::Unlocked;
    }

    // Hooks assigned by the Hub app (both default to no-ops).
    std::function<void(const std::string& appName)> onAccessRequested =
        [](const std::string&) {};
    std::function<void(const UnlockUpdate&, const std::vector<std::string>&)>
        onDecisionChanged = [](const UnlockUpdate&, const std::vector<std::string>&)
    {
    };
    std::function<void()> onFocus = [] {};

private:
    UnlockDecision snapshotLocked() const
    {
        return {.decision = current, .message = describe(current)};
    }

    mutable std::mutex stateMutex;
    Decision current = Decision::Standby;
    std::string requestedApp;
    std::vector<std::string> subscribers;
};

// ---- App-side API -----------------------------------------------------
//
// One instance lives in each app. The Hub calls notifyDecision on it when
// the decision changes; the app wires onUpdate to reveal its feature.
class ClientApi
{
public:
    void reflect(Miro::ApiReflector& reflector)
    {
        reflector.command(&ClientApi::notifyDecision, "notifyDecision");
        reflector.command(&ClientApi::focus, "focus");
    }

    Ack notifyDecision(const UnlockUpdate& update)
    {
        onUpdate(update);
        return {};
    }

    // Single-instance: raise the running window (see GatingApi::focus).
    Ack focus()
    {
        onFocus();
        return {};
    }

    std::function<void(const UnlockUpdate&)> onUpdate = [](const UnlockUpdate&) {};
    std::function<void()> onFocus = [] {};
};

} // namespace hub
