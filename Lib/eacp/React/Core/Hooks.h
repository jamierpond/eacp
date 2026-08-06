#pragma once

#include "Element.h"

#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>

namespace eacp::React
{
// State that outlives a render, kept on the mounted instance and found by call
// order.
//
// Which is the whole reason the rules of hooks exist, here as much as in React:
// there is no name in `useState(0)` to look the slot up by, so the *nth* call
// during a render is the nth slot, and a hook behind an `if` shifts every hook
// after it onto somebody else's storage. Call them unconditionally, at the top,
// always in the same order.
class HookSlot
{
public:
    virtual ~HookSlot() = default;

    // Run when the instance holding this is unmounted. Only an effect has
    // anything to do here, and what it does is its cleanup.
    virtual void unmount() {}
};

template <typename T>
class StateSlot final : public HookSlot
{
public:
    T value {};
};

template <typename T>
class RefSlot final : public HookSlot
{
public:
    T value {};
};

template <typename T>
class MemoSlot final : public HookSlot
{
public:
    Deps dependencies;
    std::optional<T> value;
};

using Cleanup = std::function<void()>;

class EffectSlot final : public HookSlot
{
public:
    void unmount() override
    {
        cleanup();
        cleanup = [] {};
    }

    Deps dependencies;
    Cleanup cleanup = [] {};

    // The run itself, queued by the render and performed by the commit -- an
    // effect must not run while the tree it is looking at is half-built.
    std::function<void()> pending;
};

namespace Detail
{
// The instance being rendered right now. Only ever called from inside a render
// function, which is what makes a hook outside one a hard error rather than a
// silent write into the wrong instance.
Instance& renderingInstance();

void beginRender(Instance& instance);
void endRender();

// Renders nest -- reconciling one function component's output reaches the next
// one down -- and a render that throws must still leave the stack as it found
// it, or every hook after it lands on the wrong instance.
class ScopedRender
{
public:
    explicit ScopedRender(Instance& instance) { beginRender(instance); }
    ~ScopedRender() { endRender(); }

    ScopedRender(const ScopedRender&) = delete;
    ScopedRender& operator=(const ScopedRender&) = delete;
};

int takeHookIndex();

HookSlot* hookAt(Instance& instance, int index);
void putHook(Instance& instance, int index, HookSlot* slot);

// A token that says whether `instance` is still mounted, for a setter captured
// into a callback that may outlive it.
std::shared_ptr<Instance*> livenessToken(Instance& instance);

void stateChanged(Instance& instance);
void scheduleEffect(EffectSlot& slot);

std::shared_ptr<const void> findContext(Instance& instance, const void* tag);

template <typename SlotType>
std::pair<SlotType&, bool> useSlot()
{
    auto& instance = renderingInstance();
    auto index = takeHookIndex();

    if (auto* existing = hookAt(instance, index))
        return {static_cast<SlotType&>(*existing), false};

    auto* created = new SlotType();
    putHook(instance, index, created);

    return {*created, true};
}

template <typename T>
concept Comparable = requires(const T& a, const T& b) {
    { a == b } -> std::convertible_to<bool>;
};
} // namespace Detail

// Writes a piece of state and asks for a re-render, unless the value it was
// given is the one already there.
//
// Safe to outlive its component: it holds a weak token rather than a pointer,
// so a callback fired after an unmount does nothing instead of writing through
// freed storage.
template <typename T>
class Setter
{
public:
    Setter() = default;

    Setter(std::shared_ptr<Instance*> tokenToUse, int indexToUse)
        : token(std::move(tokenToUse))
        , index(indexToUse)
    {
    }

    void operator()(T next) const
    {
        write([&next](T& value) { value = std::move(next); });
    }

    // The functional form, for an update that reads what is there:
    // setCount([](int current) { return current + 1; }). Two of these in one
    // event handler compose, where two plain sets would not -- both are applied
    // before the single re-render they share.
    void operator()(const std::function<T(const T&)>& update) const
    {
        write([&update](T& value) { value = update(value); });
    }

private:
    template <typename Mutate>
    void write(Mutate mutate) const
    {
        auto locked = token.lock();

        if (locked == nullptr || *locked == nullptr)
            return;

        auto& instance = **locked;
        auto* slot = static_cast<StateSlot<T>*>(Detail::hookAt(instance, index));

        if (slot == nullptr)
            return;

        if constexpr (std::is_copy_constructible_v<T> && Detail::Comparable<T>)
        {
            auto previous = slot->value;
            mutate(slot->value);

            if (previous == slot->value)
                return;
        }
        else
        {
            mutate(slot->value);
        }

        Detail::stateChanged(instance);
    }

    std::weak_ptr<Instance*> token;
    int index = 0;
};

// Exactly two members, so `auto [count, setCount] = useState(0)` reads the way
// the original does. A third would be more expressive and would break every
// call site that binds two names.
template <typename T>
struct StateHook
{
    T value;
    Setter<T> set;
};

template <typename T>
StateHook<T> useState(T initial)
{
    auto& instance = Detail::renderingInstance();
    auto index = Detail::takeHookIndex();

    auto* slot = static_cast<StateSlot<T>*>(Detail::hookAt(instance, index));

    if (slot == nullptr)
    {
        slot = new StateSlot<T>();
        slot->value = std::move(initial);
        Detail::putHook(instance, index, slot);
    }

    return {slot->value, Setter<T> {Detail::livenessToken(instance), index}};
}

// Runs `effect` after the tree is mounted and laid out, never during a render.
//
// Return a callable from it to have that run before the next run and at
// unmount -- a timer stopped, a listener taken off, a request cancelled. Return
// nothing and there is nothing to undo.
//
// The dependencies decide when it runs again: none written at all is every
// render, deps() is once, deps(a, b) is whenever either changes.
template <typename Fn>
void useEffect(Fn effect, Deps dependencies = {})
{
    auto [slot, created] = Detail::useSlot<EffectSlot>();

    auto changed =
        created || dependencies.always || !(slot.dependencies == dependencies);

    slot.dependencies = std::move(dependencies);

    if (!changed)
        return;

    auto* target = &slot;

    target->pending = [target, effect = std::move(effect)]() mutable
    {
        target->cleanup();

        if constexpr (std::is_void_v<std::invoke_result_t<Fn&>>)
        {
            effect();
            target->cleanup = [] {};
        }
        else
        {
            target->cleanup = effect();
        }
    };

    Detail::scheduleEffect(slot);
}

// A value that survives renders and changing it does *not* re-render: a
// scratchpad, a running total, the previous value of something. What state is
// not for.
template <typename T>
T& useRef(T initial = T {})
{
    auto [slot, created] = Detail::useSlot<RefSlot<T>>();

    if (created)
        slot.value = std::move(initial);

    return slot.value;
}

template <typename Fn>
auto& useMemo(Fn compute, Deps dependencies)
{
    using Value = std::invoke_result_t<Fn&>;

    auto [slot, created] = Detail::useSlot<MemoSlot<Value>>();

    auto changed =
        created || dependencies.always || !(slot.dependencies == dependencies);

    slot.dependencies = std::move(dependencies);

    if (changed)
        slot.value = compute();

    return *slot.value;
}

// The nearest value of this type provided above, or null where there is none.
template <typename T>
const T* useContext()
{
    auto found =
        Detail::findContext(Detail::renderingInstance(), contextTagFor<T>());

    return static_cast<const T*>(found.get());
}

template <typename T>
const T& useContext(const T& fallback)
{
    auto* found = useContext<T>();

    return found != nullptr ? *found : fallback;
}
} // namespace eacp::React
