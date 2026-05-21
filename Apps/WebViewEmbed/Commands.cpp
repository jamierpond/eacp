#include "Types.h"

#include <eacp/WebView/WebView.h>

#include <ea_data_structures/Pointers/Broadcaster.h>

#include <algorithm>
#include <cmath>

namespace
{

class ParametersStore
{
public:
    const Parameters& get() const { return params; }
    EA::Broadcaster& getBroadcaster() { return broadcaster; }

    void set(const Parameters& next)
    {
        params.level = std::clamp(next.level, 0.0, 1.0);
        params.autoCycle = next.autoCycle;
        cyclePhase = std::asin(params.level * 2.0 - 1.0);
        broadcaster.trigger();
    }

    void advanceTick()
    {
        params.counter++;

        if (params.autoCycle)
        {
            cyclePhase += 0.05;
            params.level = 0.5 + 0.5 * std::sin(cyclePhase);
        }

        broadcaster.trigger();
    }

private:
    Parameters params;
    EA::Broadcaster broadcaster;
    double cyclePhase = 0.0;
};

} // namespace

ParametersStore& parametersStore()
{
    static auto store = ParametersStore {};
    return store;
}

EACP_STATE(Parameters, parametersStore, parameters)

Parameters getParameters()
{
    return parametersStore().get();
}

void setParameters(const Parameters& req)
{
    parametersStore().set(req);
}

void advanceTick()
{
    parametersStore().advanceTick();
}

MIRO_EXPORT_COMMANDS(getParameters, setParameters)
