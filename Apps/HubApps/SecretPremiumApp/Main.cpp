#include "../GatingApi.h"
#include "../Launcher.h"
#include "../Ui.h"

#include "Peer.h"

#include <eacp/Graphics/Graphics.h>

#include <Miro/Miro.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <string>
#include <thread>

using namespace eacp;
using namespace eacp::Graphics;
using namespace hub;
using namespace std::chrono_literals;

namespace
{
const char* gExecutablePath = "";
} // namespace

// The "dope-ass feature": a gradient panel with drifting, pulsing orbs and
// a shimmering title, animated off the DisplayLink. Inert until start().
struct FeatureView final : View
{
    FeatureView()
    {
        titleLayer->setText("✦ PREMIUM UNLOCKED ✦");
        titleLayer->setFont(FontOptions().withName("Helvetica-Bold").withSize(26.f));
        titleLayer->setColor(ui::text);

        subtitleLayer->setText("welcome to the dope-ass feature");
        subtitleLayer->setColor(ui::text);

        const char* perks[perkCount] = {
            "Quantum-smoothed gradients",
            "Buttery 120fps everything",
            "Infinite undo across timelines",
            "Zero-latency collaborative cursors",
        };

        // Panel first (bottom), then orbs on top of it, then the text on
        // top of everything — z-order follows insertion order.
        addChildren({panelLayer});

        for (auto& orb: orbs)
        {
            orb.setOpacity(0.4f);
            addLayer(orb);
        }
        orbs[0].setFillColor(ui::accent);
        orbs[1].setFillColor({0.65f, 0.35f, 0.95f});
        orbs[2].setFillColor(ui::good);

        addChildren({titleLayer, subtitleLayer});

        for (auto i = 0; i < perkCount; ++i)
        {
            perkLayers[i]->setText(std::string {"➜  "} + perks[i]);
            perkLayers[i]->setColor(ui::text);
            addChildren({perkLayers[i]});
        }
    }

    void start() { started = true; }

    void update(Threads::FrameTime frame)
    {
        if (!started)
            return;

        auto time = static_cast<float>(frame.time);
        auto bounds = getLocalBounds();

        for (auto i = 0; i < orbCount; ++i)
        {
            auto fi = static_cast<float>(i);
            auto phase = fi * 2.1f;
            auto drift = 0.5f + 0.32f * std::sin(time * (0.6f + 0.2f * fi) + phase);
            auto rise = 0.5f + 0.30f * std::cos(time * (0.5f + 0.15f * fi) + phase);
            orbs[i].setOpacity(0.28f + 0.22f * std::sin(time * 1.6f + phase));
            orbs[i].setPosition({drift * bounds.w, rise * bounds.h});
        }

        auto shimmer = 0.75f + 0.25f * std::sin(time * 2.4f);
        titleLayer->setColor({0.55f + 0.45f * shimmer, 0.8f * shimmer, 1.0f});
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        auto panel = Path();
        panel.addRoundedRect(bounds.getRelative({0.05f, 0.06f, 0.9f, 0.88f}), 16.f);
        panelLayer->setPath(panel);

        auto gradient = LinearGradient({0.f, 0.f},
                                       {bounds.w, bounds.h},
                                       {{{0.12f, 0.16f, 0.34f}, 0.f},
                                        {{0.22f, 0.12f, 0.36f}, 0.55f},
                                        {{0.10f, 0.20f, 0.32f}, 1.f}});
        panelLayer->setFillGradient(gradient);
        panelLayer.scaleToFit();

        for (auto& orb: orbs)
        {
            orb.setBounds(bounds);
            auto path = Path();
            path.addEllipse({-38.f, -38.f, 76.f, 76.f});
            orb.setPath(path);
        }

        scaleToFit({titleLayer, subtitleLayer});
        scaleToFit({perkLayers[0], perkLayers[1], perkLayers[2], perkLayers[3]});

        auto left = bounds.w * 0.12f;
        titleLayer->setPosition({left, bounds.h - 92.f});
        subtitleLayer->setPosition({left, bounds.h - 120.f});

        for (auto i = 0; i < perkCount; ++i)
            perkLayers[i]->setPosition(
                {left, bounds.h - 162.f - static_cast<float>(i) * 26.f});
    }

    static constexpr int orbCount = 3;
    static constexpr int perkCount = 4;

    bool started = false;

    ShapeLayerView panelLayer;
    TextLayerView titleLayer;
    TextLayerView subtitleLayer;
    TextLayerView perkLayers[perkCount];
    ShapeLayer orbs[orbCount];

    Threads::DisplayLink link {[this](Threads::FrameTime frame) { update(frame); }};
};

// Root view: serves a ClientApi callback endpoint on its own peer, finds
// the Hub via the rendezvous file (launching it if needed), subscribes,
// and reveals the feature once the Hub reports Unlocked — either from the
// requestUnlock reply or asynchronously via the notifyDecision callback.
struct AppView final : View
{
    AppView()
    {
        backgroundLayer->setFillColor(ui::background);

        titleLayer->setText("SecretPremiumApp");
        titleLayer->setFont(FontOptions().withName("Helvetica-Bold").withSize(24.f));
        titleLayer->setColor(ui::text);

        statusLayer->setText("Contacting the Hub…");
        statusLayer->setColor(ui::faint);

        hintLayer->setColor(ui::faint);

        // The Hub's notifyDecision lands here (on the app's HTTP dispatcher);
        // marshal onto the UI thread to reveal.
        clientApi.onUpdate = [this](const UnlockUpdate& update)
        {
            if (update.decision == Decision::Unlocked)
                Threads::callAsync([this] { reveal(); });
        };
        peer.serve(clientApi);
        rpc::writeEndpoint("secretapp", peer.baseUrl());

        addChildren({backgroundLayer, titleLayer, statusLayer, hintLayer});

        worker = std::thread([this] { workerBody(); });
    }

    ~AppView() override
    {
        stopping = true;
        if (worker.joinable())
            worker.join();
        rpc::removeEndpoint("secretapp");
    }

    void setStatus(const std::string& textToUse, const Color& color)
    {
        statusLayer->setText(textToUse);
        statusLayer->setColor(color);
    }

    void reveal()
    {
        if (revealed.exchange(true))
            return;

        removeSubview(titleLayer);
        removeSubview(statusLayer);
        removeSubview(hintLayer);
        addSubview(feature);
        feature.start();
        resized();
    }

    void deliver(const UnlockDecision& decision)
    {
        if (decision.decision == Decision::Unlocked)
        {
            reveal();
        }
        else
        {
            setStatus("Locked — " + decision.message, ui::accent);
            hintLayer->setText("Type 42 in the Hub window to unlock.");
        }
    }

    void workerBody()
    {
        auto request = AppUnlockRequest {.appName = "SecretPremiumApp"};
        auto subscribeRequest = SubscribeRequest {.appName = "SecretPremiumApp",
                                                  .callbackUrl = peer.baseUrl()};
        auto launched = false;

        for (auto attempt = 0; attempt < 40 && !stopping; ++attempt)
        {
            if (auto url = rpc::readEndpoint("hub"))
            {
                try
                {
                    // Subscribe first, so any unlock after this point reaches
                    // us via the callback, then read the current decision.
                    peer.call<Ack>(*url, "subscribe", subscribeRequest);
                    post(peer.call<UnlockDecision>(*url, "requestUnlock", request));
                    return;
                }
                catch (const std::exception&)
                {
                    // Endpoint stale or Hub still starting — fall through.
                }
            }

            if (!launched)
            {
                Threads::callAsync(
                    [this] { setStatus("Hub not running — launching it…", ui::accent); });
                hub::launchDetached(
                    hub::siblingExecutable(gExecutablePath, "Hub", "Hub"));
                launched = true;
            }

            std::this_thread::sleep_for(250ms);
        }

        Threads::callAsync([this] { setStatus("Could not reach the Hub.", ui::bad); });
    }

    void post(const UnlockDecision& decision)
    {
        Threads::callAsync([this, decision] { deliver(decision); });
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        auto bg = Path();
        bg.addRect(bounds);
        backgroundLayer->setPath(bg);
        backgroundLayer.scaleToFit();

        feature.setBounds(bounds);

        scaleToFit({titleLayer, statusLayer, hintLayer});
        titleLayer->setPosition({40.f, bounds.h / 2.f + 30.f});
        statusLayer->setPosition({40.f, bounds.h / 2.f - 6.f});
        hintLayer->setPosition({40.f, bounds.h / 2.f - 34.f});
    }

    // Peer / threads declared first so they outlive the views/handlers.
    ClientApi clientApi;
    rpc::Peer peer;
    std::atomic<bool> revealed {false};
    std::atomic<bool> stopping {false};

    ShapeLayerView backgroundLayer;
    TextLayerView titleLayer;
    TextLayerView statusLayer;
    TextLayerView hintLayer;
    FeatureView feature;

    std::thread worker;
};

namespace
{
WindowOptions appWindowOptions()
{
    auto options = WindowOptions {};
    options.width = 560;
    options.height = 420;
    options.title = "Secret Premium App";
    options.backgroundColor = ui::background;
    return options;
}
} // namespace

struct SecretApp
{
    SecretApp()
    {
        view.clientApi.onFocus = [this]
        { Threads::callAsync([this] { window.toFront(); }); };

        window.setContentView(view);
        window.toFront();
    }

    AppView view;
    Window window {appWindowOptions()};
};

int main(int, char** argv)
{
    gExecutablePath = argv[0];

    // Single instance: if the app is already running, raise it and exit.
    if (hub::rpc::focusRunningInstance("secretapp"))
        return 0;

    eacp::Apps::run<SecretApp>();
    return 0;
}
