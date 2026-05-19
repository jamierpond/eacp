#pragma once

#include <eacp/Core/Utils/StateValue.h>

#include <Miro/Miro.h>

struct Parameters
{
    double level = 0.5;
    bool autoCycle = false;
    long long counter = 0;

    MIRO_REFLECT(level, autoCycle, counter)
};

eacp::StateValue<Parameters>& parametersState();

Parameters getParameters();
void setParameters(const Parameters& req);
void advanceTick();
