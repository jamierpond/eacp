#include "../GatingApi.h"
#include "../Launcher.h"
#include "../Ui.h"

#include "Peer.h"

#include <eacp/Graphics/Graphics.h>

#include <Miro/Miro.h>

#include <string>
#include <thread>
#include <vector>

using namespace eacp;
using namespace eacp::Graphics;
using namespace hub;

namespace
{
const char* gExecutablePath = "";
} // namespace

// The Hub window: a password gate that serves the GatingApi over local
// HTTP and, once unlocked, pushes the decision to every subscribed app and
// can launch the premium app. All UI runs on the main thread; the HTTP
// server's handlers hop here via Threads::callAsync.
struct HubView final : View
{
    HubView()
        : passwordField(FontOptions().withSize(16.f))
        , unlockButton("Unlock")
        , launchButton("Launch Premium App")
    {
        peer.serve(api);
        rpc::writeEndpoint("hub", peer.baseUrl());

        backgroundLayer->setFillColor(ui::background);

        titleLayer->setText("EACP HUB");
        titleLayer->setFont(FontOptions().withName("Helvetica-Bold").withSize(28.f));
        titleLayer->setColor(ui::text);

        subtitleLayer->setText("product gating  ·  the password is 42");
        subtitleLayer->setColor(ui::faint);

        statusLayer->setText("Suite locked. Enter the password to unlock.");
        statusLayer->setColor(ui::faint);

        passwordField.setPlaceholder("Enter the password");
        passwordField.setBackgroundColor({0.10f, 0.11f, 0.14f});
        passwordField.setTextColor(ui::text);
        passwordField.setBorderColor(ui::faint);
        passwordField.onSubmit([this](const std::string&) { tryUnlock(); });

        unlockButton.onClick = [this] { tryUnlock(); };
        launchButton.setColor(ui::good);
        launchButton.onClick = [this] { launchPremiumApp(); };

        // Handlers run on the HTTP dispatcher — marshal to the UI thread.
        api.onAccessRequested = [this](const std::string& appName)
        { Threads::callAsync([this, appName] { onAccessRequested(appName); }); };

        api.onDecisionChanged =
            [this](const UnlockUpdate& update, const std::vector<std::string>& subs)
        { notifySubscribers(update, subs); };

        addChildren({backgroundLayer, titleLayer, subtitleLayer, statusLayer,
                     passwordField, unlockButton});
    }

    ~HubView() override { rpc::removeEndpoint("hub"); }

    void tryUnlock()
    {
        auto decision = api.submitPassword({.password = passwordField.getText()});

        if (decision.decision == Decision::Unlocked)
        {
            statusLayer->setText("Unlocked ✓  subscribers notified.");
            statusLayer->setColor(ui::good);
            showUnlockedUi();
        }
        else
        {
            statusLayer->setText("Wrong password. Try again (hint: 42).");
            statusLayer->setColor(ui::bad);
            passwordField.setText("");
        }
    }

    // Fan out the decision to every subscribed app on a detached thread so
    // the UI never blocks on the outbound HTTP calls.
    void notifySubscribers(const UnlockUpdate& update,
                           const std::vector<std::string>& subs)
    {
        auto worker = std::thread(
            [this, update, subs]
            {
                for (const auto& url: subs)
                {
                    try
                    {
                        peer.call<Ack>(url, "notifyDecision", update);
                    }
                    catch (const std::exception&)
                    {
                        // Subscriber gone; skip it.
                    }
                }
            });
        worker.detach();
    }

    void launchPremiumApp()
    {
        auto exe = hub::siblingExecutable(gExecutablePath,
                                          "SecretPremiumApp",
                                          "SecretPremiumApp");

        if (hub::launchDetached(exe))
            statusLayer->setText("Launched the premium app.");
        else
            statusLayer->setText("Could not launch it — is it built?");

        statusLayer->setColor(ui::faint);
    }

    void onAccessRequested(const std::string& appName)
    {
        if (api.isUnlocked())
            return;

        statusLayer->setText("'" + appName
                             + "' wants access — enter the password.");
        statusLayer->setColor(ui::accent);
        passwordField.focus();
    }

    void showUnlockedUi()
    {
        removeSubview(passwordField);
        removeSubview(unlockButton);
        addSubview(launchButton);
        resized();
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        auto bg = Path();
        bg.addRect(bounds);
        backgroundLayer->setPath(bg);
        backgroundLayer.scaleToFit();

        scaleToFit({titleLayer, subtitleLayer, statusLayer});
        titleLayer->setPosition({36.f, bounds.h - 62.f});
        subtitleLayer->setPosition({38.f, bounds.h - 88.f});
        statusLayer->setPosition({38.f, 152.f});

        auto fieldWidth = bounds.w - 76.f;
        passwordField.setBounds({38.f, 96.f, fieldWidth, 40.f});
        unlockButton.setBounds({38.f, 36.f, fieldWidth, 46.f});
        launchButton.setBounds({38.f, 60.f, fieldWidth, 50.f});
    }

    // State / IPC declared first so they outlive the views that use them.
    GatingApi api;
    rpc::Peer peer;

    ShapeLayerView backgroundLayer;
    TextLayerView titleLayer;
    TextLayerView subtitleLayer;
    TextLayerView statusLayer;
    TextInput passwordField;
    ui::Button unlockButton;
    ui::Button launchButton;
};

namespace
{
WindowOptions hubWindowOptions()
{
    auto options = WindowOptions {};
    options.width = 460;
    options.height = 300;
    options.title = "EACP Hub";
    options.backgroundColor = ui::background;
    return options;
}
} // namespace

struct HubApp
{
    HubApp()
    {
        // A second launch (handled in main) tells this instance to focus.
        view.api.onFocus = [this]
        { Threads::callAsync([this] { window.toFront(); }); };

        window.setContentView(view);
        window.toFront();
    }

    HubView view;
    Window window {hubWindowOptions()};
};

int main(int, char** argv)
{
    gExecutablePath = argv[0];

    // Single instance: if a Hub is already running, raise it and exit.
    if (hub::rpc::focusRunningInstance("hub"))
        return 0;

    eacp::Apps::run<HubApp>();
    return 0;
}
