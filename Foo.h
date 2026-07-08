#include <functional>
#include <string>
enum class Decision
{
    UNLOCKED,
    STANDBY,
    MUST_UPDATE,
    LOGIN_REQUIRED,
    UNKNOWN_ERROR, // must reinstall hub / app suite -- damaged.
};

struct AppUnlockRequest
{
    // something representing the app request
};

struct ProductGatingServer {
  using DecisionCallback = std::function<void(Decision)>;
  DecisionCallback cb;

  void setOnUpdate(DecisionCallback fn) {
    cb = std::move(fn);
  }

  Decision processDecisionRequest(AppUnlockRequest unlockRequest) {
    // make a decision!
    return {};
  }
};

struct ProductGatingClient {
  ProductGatingClient() {
    server.setOnUpdate([this](Decision decision) {
        // accept async updates, the user may be typing a password
        currentDecision = decision;
    });
  }

  Decision requestUnlockState() {
      // start hub process if needed,
      // call over the ipc
      currentDecision = server.processDecisionRequest({});
      return currentDecision;
  }

  bool isUnlocked() const {
    return currentDecision == Decision::UNLOCKED;
  }

  // could be waiting
  ProductGatingServer server{}; // you know what i mean
  Decision currentDecision{};
};

