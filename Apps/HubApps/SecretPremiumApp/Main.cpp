#include "../Launcher.h"
#include "../Protocol.h"
#include "../Ui.h"

#include "NngRpc.h"

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

        // Give each text view the full panel width so CATextLayer doesn't
        // truncate the strings with an ellipsis.
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

// Root view: shows a locked/waiting state, launches the Hub if needed
// (on a worker thread so the blocking IPC never freezes the UI), and
// reveals the feature once the Hub reports Unlocked — synchronously from
// the request or asynchronously via the pub/sub broadcast.
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

        addChildren({backgroundLayer, titleLayer, statusLayer, hintLayer});

        worker = std::thread([this] { workerBody(); });
    }

    ~AppView() override
    {
        stopping = true;
        if (worker.joinable())
            worker.join();
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

    // Runs on the main thread (posted from the worker / subscriber).
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

        // Quick probe: is the Hub already up?
        client.setTimeoutMs(800);
        try
        {
            post(client.invoke<UnlockDecision>("requestUnlock", request));
            return;
        }
        catch (const std::exception&)
        {
        }

        Threads::callAsync(
            [this] { setStatus("Hub not running — launching it…", ui::accent); });
        hub::launchDetached(hub::siblingExecutable(gExecutablePath, "Hub", "Hub"));

        client.setTimeoutMs(8000);
        for (auto attempt = 0; attempt < 20 && !stopping; ++attempt)
        {
            try
            {
                post(client.invoke<UnlockDecision>("requestUnlock", request));
                return;
            }
            catch (const std::exception&)
            {
                std::this_thread::sleep_for(300ms);
            }
        }

        Threads::callAsync([this]
                           { setStatus("Could not reach the Hub.", ui::bad); });
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

    // IPC / threads declared so the client outlives the workers using it.
    hub::ipc::RpcClient client {hub::rpcUrl};
    std::atomic<bool> revealed {false};
    std::atomic<bool> stopping {false};

    ShapeLayerView backgroundLayer;
    TextLayerView titleLayer;
    TextLayerView statusLayer;
    TextLayerView hintLayer;
    FeatureView feature;

    std::thread worker;

    // Subscriber declared last so it destructs first, stopping its thread
    // before the views its handler touches are torn down.
    hub::ipc::Subscriber subscriber {
        hub::eventUrl,
        [this](const std::string& event, const Miro::JSON& payload)
        {
            if (event != "decisionChanged")
                return;

            auto update = UnlockUpdate {};
            Miro::fromJSON(update, payload);

            if (update.decision == Decision::Unlocked)
                Threads::callAsync([this] { reveal(); });
        }};
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
        window.setContentView(view);
        window.toFront();
    }

    AppView view;
    Window window {appWindowOptions()};
};

int main(int, char** argv)
{
    gExecutablePath = argv[0];
    eacp::Apps::run<SecretApp>();
    return 0;
}
