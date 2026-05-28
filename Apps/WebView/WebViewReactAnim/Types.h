#pragma once

#include <Miro/Miro.h>

#include <chrono>
#include <cmath>

// Tick stays at file scope (not in an Api:: namespace) so its
// qualifiedName matches the baseline TS output exactly — switching the
// codegen path mustn't change the wire format.
struct Tick
{
    double angle = 0.0;

    MIRO_REFLECT(angle)
};

// Frame counter surfaced by the stats sub-API below. File scope (like
// Tick) so its wire/qualified name is just "Stats".
struct Stats
{
    int ticks = 0;

    MIRO_REFLECT(ticks)
};

namespace Api
{
using namespace std::chrono;

// A sub-API composed into Clock via MIRO_API(r, stats). It exists to
// demonstrate (and exercise) eacp's nested-API codegen: its command and
// event land on the wire under the member name as "stats::getStats" and
// "stats::updated", which the TS generators surface as the nested
// backend.stats.getStats() call and the quoted backend.on("stats::updated")
// event key. Clock's flat top-level getCurrentTick / tick sit alongside
// to show both forms coexist.
class StatsApi
{
public:
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    void reflect(Miro::ApiReflector& r)
    {
        using T = StatsApi;

        r.commands<&T::getStats>();
        r.events<&T::updated>();
    }

    Stats getStats() const { return current; }

    // Called once per tick by Clock::update(). Throttles the push to a
    // few Hz so the bridge isn't flooded at the 120Hz tick rate.
    void recordTick()
    {
        current.ticks++;
        if (current.ticks % 30 == 0)
            updated.publish(current);
    }

    // Push channel for the running frame count.
    Miro::Event<Stats> updated;

private:
    Stats current;
};

// The whole WebViewReactAnim API. The `reflect` method is the single
// source of truth for codegen (DescribeReflector walks it) and runtime
// (BindReflector walks the same body to install handlers + subscribe
// to the tick event). Replaces the MIRO_EXPORT_COMMAND(getCurrentTick)
// + EACP_EVENT(tick, Tick) pair the static-init path used. The stats
// member is composed in via MIRO_API, namespacing its commands/events
// under "stats::".
class Clock
{
public:
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    void reflect(Miro::ApiReflector& r)
    {
        using T = Clock;

        r.commands<&T::getCurrentTick>();
        r.events<&T::tick>();
        MIRO_API(r, stats)
    }

    Tick getCurrentTick() const
    {
        auto seconds = duration<double>(steady_clock::now() - startTime).count();
        return {.angle = std::fmod(seconds * 90.0, 360.0)};
    }

    void update()
    {
        tick.publish(getCurrentTick());
        stats.recordTick();
    }

    // Push channel for tick updates. The MyApp timer publishes here;
    // the transport's BindReflector listener forwards each payload over
    // the WebView bridge.
    Miro::Event<Tick> tick;

    // Nested sub-API: its commands/events are exposed under "stats::".
    StatsApi stats;

private:
    steady_clock::time_point startTime = steady_clock::now();
};

} // namespace Api
