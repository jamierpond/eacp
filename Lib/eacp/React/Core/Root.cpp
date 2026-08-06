#include "Root.h"

#include "../Host/Elements.h"
#include "../Layout/Layout.h"

#include <eacp/Core/Threads/EventLoop.h>

#include <algorithm>

namespace eacp::React
{
namespace
{
int depthOf(const Instance* instance)
{
    auto depth = 0;

    for (const auto* node = instance; node != nullptr; node = node->parent)
        ++depth;

    return depth;
}

int countInstances(const Instance& instance)
{
    auto total = 1;

    for (const auto& child: instance.children)
        total += countInstances(*child);

    return total;
}

// Whether an instance can be kept for `element` or has to be replaced. The type
// tag answers it on its own: a HostType address for a host, and the address of
// a per-closure-type static for a function, so two lambdas that happen to be in
// the same position never inherit each other's hooks.
bool canReuse(const Instance& instance, const Element& element)
{
    return instance.element.kind == element.kind
           && instance.element.typeTag == element.typeTag;
}
} // namespace

Root::Root(std::function<Element()> appToUse)
    : app(std::move(appToUse))
{
    // The window's one child is a function component, so the top of the tree is
    // an ordinary render function with ordinary hooks -- there is nothing
    // special about being the root.
    auto appElement = component([this] { return app(); });

    auto rootChildren = Children {};
    rootChildren.add(std::move(appElement));

    tree = mount(Column {}(std::move(rootChildren)), nullptr, nullptr);
    appInstance = tree->children[0].get();

    setRootComponent(*tree->component);

    // Nothing has a size yet, so the effects the first render queued wait for
    // the flush that follows the first layout.
    structureChanged = false;
    scheduleFlush();
}

Root::~Root()
{
    *alive = nullptr;

    // Before the ComponentHost base goes: unmounting runs effect cleanups and
    // destroys components, and both may still reach the host.
    tree = nullptr;
}

void Root::setRootStyle(const Style& style)
{
    rootStyle = style;

    if (tree != nullptr && tree->component != nullptr)
    {
        static_cast<BoxView&>(*tree->component).setStyle(rootStyle);
        performLayout();
    }
}

int Root::getMountedInstanceCount() const
{
    return tree != nullptr ? countInstances(*tree) : 0;
}

OwningPointer<Instance>
    Root::mount(const Element& element, Instance* parent, UI::Component* hostParent)
{
    auto instance = OwningPointer<Instance> {new Instance()};

    *instance->alive = instance.get();
    instance->root = this;
    instance->parent = parent;
    instance->hostParent = hostParent;
    instance->element = element;
    instance->contextTag = element.contextTag;
    instance->contextValue = element.contextValue;

    switch (element.kind)
    {
        case Element::Kind::Host:
        {
            instance->hostType = hostTypeOf(element);
            instance->props = element.props;
            instance->component = instance->hostType->create();
            instance->style = instance->hostType->styleOf(element.props.get());

            instance->hostType->apply(
                *instance->component, element.props.get(), nullptr, instance->style);

            if (hostParent != nullptr)
                hostParent->addAndMakeVisible(*instance->component);

            structureChanged = true;

            for (const auto& child: element.children)
                instance->children.add(
                    mount(child, instance.get(), instance->component.get()));

            break;
        }

        case Element::Kind::Function:
            renderFunction(*instance);
            break;

        case Element::Kind::Fragment:
            for (const auto& child: element.children)
                instance->children.add(mount(child, instance.get(), hostParent));

            break;
    }

    return instance;
}

void Root::renderFunction(Instance& instance)
{
    auto rendered = Element {};

    {
        auto scope = Detail::ScopedRender {instance};
        rendered = instance.element.render();
    }

    ++lastRenders;

    auto next = Children {};
    next.add(std::move(rendered));

    reconcileChildren(instance, next, instance.hostParent);
}

void Root::updateInstance(Instance& instance, const Element& next)
{
    instance.dirty = false;

    switch (next.kind)
    {
        case Element::Kind::Host:
        {
            // The props the component currently has, kept alive across the
            // apply so a setter can be skipped when the value has not moved.
            auto previous = instance.props;

            instance.element = next;
            instance.props = next.props;
            instance.style = instance.hostType->styleOf(next.props.get());

            instance.hostType->apply(*instance.component,
                                     next.props.get(),
                                     previous.get(),
                                     instance.style);

            reconcileChildren(instance, next.children, instance.component.get());
            break;
        }

        case Element::Kind::Function:
        {
            auto unchanged = next.memoized && instance.element.memoized
                             && instance.element.memo == next.memo;

            instance.element = next;

            if (!unchanged)
                renderFunction(instance);

            break;
        }

        case Element::Kind::Fragment:
            instance.element = next;
            instance.contextTag = next.contextTag;
            instance.contextValue = next.contextValue;

            reconcileChildren(instance, next.children, instance.hostParent);
            break;
    }
}

void Root::reconcileChildren(Instance& parent,
                             const Children& next,
                             UI::Component* hostParent)
{
    auto previous = std::move(parent.children);
    parent.children.clear();
    parent.children.reserve(next.size());

    auto taken = Vector<char>(previous.size());

    auto matchFor = [&](const Element& element, int position)
    {
        if (!element.key.empty())
        {
            for (auto index = 0; index < previous.size(); ++index)
                if (taken[index] == 0 && previous[index] != nullptr
                    && previous[index]->element.key == element.key
                    && canReuse(*previous[index], element))
                    return index;

            return -1;
        }

        // Unkeyed children match by position, which is right for a layout and
        // wrong for a list that reorders -- hence keys.
        if (position < previous.size() && taken[position] == 0
            && previous[position] != nullptr
            && previous[position]->element.key.empty()
            && canReuse(*previous[position], element))
            return position;

        return -1;
    };

    for (auto index = 0; index < next.size(); ++index)
    {
        const auto& element = next[index];
        auto match = matchFor(element, index);

        if (match < 0)
        {
            parent.children.add(mount(element, &parent, hostParent));
            structureChanged = true;
            continue;
        }

        taken[match] = 1;

        auto instance = std::move(previous[match]);
        instance->hostParent = hostParent;

        if (match != index)
            structureChanged = true;

        updateInstance(*instance, element);
        parent.children.add(std::move(instance));
    }

    for (auto index = 0; index < previous.size(); ++index)
        if (taken[index] == 0 && previous[index] != nullptr)
            structureChanged = true;

    // What is left in `previous` was not matched, and destroying it here is the
    // unmount: the instance destructor runs the effect cleanups and takes the
    // components out of the tree.
}

void Root::clearSubtreeDirty(Instance& instance)
{
    instance.dirty = false;

    for (auto& child: instance.children)
        clearSubtreeDirty(*child);
}

void Root::scheduleUpdate(Instance& instance)
{
    if (instance.dirty)
        return;

    instance.dirty = true;
    pendingUpdates.add(instance.alive);

    scheduleFlush();
}

void Root::addPendingEffect(Instance& instance, EffectSlot& slot)
{
    pendingEffects.add({instance.alive, &slot});
    scheduleFlush();
}

void Root::scheduleFlush()
{
    if (flushScheduled)
        return;

    flushScheduled = true;

    // Batched, which is what makes a handler that sets three pieces of state
    // cost one render and one layout -- and what makes it safe for a handler to
    // destroy the component it was called from, since the tree is not touched
    // until the event that reached it is over.
    Threads::callAsync(
        [token = std::weak_ptr<Root*> {alive}]
        {
            auto locked = token.lock();

            if (locked == nullptr || *locked == nullptr)
                return;

            (*locked)->flush();
        });
}

void Root::flush()
{
    flushScheduled = false;
    lastRenders = 0;

    if (!pendingUpdates.empty())
    {
        auto queued = std::move(pendingUpdates);
        pendingUpdates.clear();

        // Shallowest first, so an ancestor's re-render subsumes its
        // descendants' rather than each of them rendering twice.
        std::sort(queued.begin(),
                  queued.end(),
                  [](const auto& a, const auto& b)
                  { return depthOf(*a) < depthOf(*b); });

        for (auto& token: queued)
        {
            auto* instance = *token;

            if (instance == nullptr || !instance->dirty)
                continue;

            renderFunction(*instance);
            clearSubtreeDirty(*instance);
        }
    }

    if (structureChanged)
    {
        restack(*tree);
        structureChanged = false;
    }

    performLayout();
    runPendingEffects();
}

void Root::renderNow()
{
    if (appInstance != nullptr)
    {
        renderFunction(*appInstance);
        clearSubtreeDirty(*appInstance);
    }

    if (structureChanged)
    {
        restack(*tree);
        structureChanged = false;
    }

    performLayout();
    runPendingEffects();
}

void Root::restack(Instance& instance)
{
    if (instance.component != nullptr && instance.hostParent != nullptr)
        instance.component->toFront();

    for (auto& child: instance.children)
        restack(*child);
}

void Root::performLayout()
{
    if (tree == nullptr)
        return;

    auto bounds = getLocalBounds();

    if (bounds.w <= 0.f || bounds.h <= 0.f)
        return;

    tree->style = rootStyle;

    Layout::perform(*tree, bounds);
}

void Root::runPendingEffects()
{
    auto queued = std::move(pendingEffects);
    pendingEffects.clear();

    for (auto& entry: queued)
    {
        if (*entry.token == nullptr || entry.slot->pending == nullptr)
            continue;

        auto run = std::move(entry.slot->pending);
        entry.slot->pending = nullptr;

        run();
    }
}

void Root::resized()
{
    UI::ComponentHost::resized();

    performLayout();
}
} // namespace eacp::React
