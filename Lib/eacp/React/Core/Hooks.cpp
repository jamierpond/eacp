#include "Root.h"

namespace eacp::React::Detail
{
namespace
{
// Which instance a hook belongs to, which is only ever "the one being rendered
// right now". A stack rather than a pointer because renders nest.
Vector<Instance*>& renderStack()
{
    static auto stack = Vector<Instance*> {};

    return stack;
}
} // namespace

Instance& renderingInstance()
{
    return *renderStack().back();
}

void beginRender(Instance& instance)
{
    instance.hookCursor = 0;
    renderStack().add(&instance);
}

void endRender()
{
    renderStack().erase(renderStack().end() - 1);
}

int takeHookIndex()
{
    return renderingInstance().hookCursor++;
}

HookSlot* hookAt(Instance& instance, int index)
{
    if (index < 0 || index >= instance.hooks.size())
        return nullptr;

    return instance.hooks[index].get();
}

void putHook(Instance& instance, int index, HookSlot* slot)
{
    while (instance.hooks.size() <= index)
        instance.hooks.add(OwningPointer<HookSlot> {});

    instance.hooks[index] = OwningPointer<HookSlot> {slot};
}

std::shared_ptr<Instance*> livenessToken(Instance& instance)
{
    return instance.alive;
}

void stateChanged(Instance& instance)
{
    if (instance.root != nullptr)
        instance.root->scheduleUpdate(instance);
}

void scheduleEffect(EffectSlot& slot)
{
    auto& instance = renderingInstance();

    if (instance.root != nullptr)
        instance.root->addPendingEffect(instance, slot);
}

std::shared_ptr<const void> findContext(Instance& instance, const void* tag)
{
    for (auto* node = &instance; node != nullptr; node = node->parent)
        if (node->contextTag == tag)
            return node->contextValue;

    return {};
}
} // namespace eacp::React::Detail
