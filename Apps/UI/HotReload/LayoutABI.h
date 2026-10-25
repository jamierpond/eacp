#pragma once

#include <eacp/UI/UI.h>

// The module binds to the app's own eacp, so C++ types cross freely. demoLayout
// only moves what the host owns, creating, installing and keeping nothing.
// Everything else in Layout.cpp stays in its anonymous namespace, or it binds
// to the app's copy.

struct LayoutTargets
{
    eacp::UI::Component& title;
    eacp::UI::Component& sidebar;
    eacp::UI::Component& gain;
    eacp::UI::Component& mute;
    eacp::UI::Component& notes;
    eacp::UI::Component& footer;
};

void demoLayout(const LayoutTargets& targets, const eacp::UI::Rect& bounds);
