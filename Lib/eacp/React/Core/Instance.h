#pragma once

#include "Hooks.h"

namespace eacp::React
{
class Root;

// A mounted node: the element that produced it, whatever real component it
// stands for, and everything that has to survive the next render.
//
// The tree of these mirrors the element tree exactly, including the nodes that
// have no component of their own -- a function component and a fragment are
// both instances with a null `component`, and their children are attached to
// the nearest host ancestor instead. Keeping them in the tree is what gives a
// function component somewhere to put its hooks.
class Instance
{
public:
    Instance() = default;
    ~Instance();

    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;

    bool isHost() const { return component != nullptr; }

    // The last element rendered here. Its props are the ones currently applied,
    // so the next render diffs against them.
    Element element;

    const HostType* hostType = nullptr;
    OwningPointer<UI::Component> component;
    std::shared_ptr<const void> props;

    // Where this instance's components go: the nearest host ancestor's
    // component, or the root container. Carried rather than walked for, since a
    // reconcile of a fragment's children needs it without having a host of its
    // own to look down from.
    UI::Component* hostParent = nullptr;

    Root* root = nullptr;
    Instance* parent = nullptr;
    Vector<OwningPointer<Instance>> children;

    Vector<OwningPointer<HookSlot>> hooks;
    int hookCursor = 0;

    // Set to this on mount and cleared in the destructor, so a Setter captured
    // into a callback can tell whether what it writes to is still there.
    std::shared_ptr<Instance*> alive = std::make_shared<Instance*>(nullptr);

    const void* contextTag = nullptr;
    std::shared_ptr<const void> contextValue;

    // Filled by the layout pass. `frame` is in the coordinate space of
    // hostParent, which is exactly what setBounds wants.
    Style style;
    Size intrinsic;
    Rect frame;

    bool dirty = false;
};
} // namespace eacp::React
