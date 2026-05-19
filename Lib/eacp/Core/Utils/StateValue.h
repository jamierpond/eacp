#pragma once

#include <ea_data_structures/Structures/Vector.h>

#include <functional>
#include <utility>

namespace eacp
{

// Observable single-value holder. Listeners are notified after every
// set() / modify(); subscriptions are RAII handles that detach on
// destruction. Lives in Core because it has no graphics / bridge
// dependency — transport adapters (e.g. bindToBridge in WebView) layer
// on top.
template <typename T>
class StateValue
{
public:
    using Listener = std::function<void(const T&)>;

    class Subscription
    {
    public:
        Subscription() = default;
        ~Subscription() { reset(); }

        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;

        Subscription(Subscription&& other) noexcept
            : state(std::exchange(other.state, nullptr))
            , id(std::exchange(other.id, 0))
        {
        }

        Subscription& operator=(Subscription&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                state = std::exchange(other.state, nullptr);
                id = std::exchange(other.id, 0);
            }
            return *this;
        }

        void reset()
        {
            if (state != nullptr)
                state->removeListener(id);
            state = nullptr;
            id = 0;
        }

    private:
        friend class StateValue;

        Subscription(StateValue& owner, int idToUse)
            : state(&owner), id(idToUse)
        {
        }

        StateValue* state = nullptr;
        int id = 0;
    };

    explicit StateValue(T initial = {}) : value(std::move(initial)) {}

    StateValue(const StateValue&) = delete;
    StateValue& operator=(const StateValue&) = delete;
    StateValue(StateValue&&) = delete;
    StateValue& operator=(StateValue&&) = delete;

    const T& get() const noexcept { return value; }

    void set(T next)
    {
        value = std::move(next);
        notify();
    }

    template <typename Mutator>
    void modify(Mutator&& mutator)
    {
        mutator(value);
        notify();
    }

    Subscription addListener(Listener listener)
    {
        auto id = ++nextId;
        listeners.add(Entry {id, std::move(listener)});
        return Subscription {*this, id};
    }

private:
    void notify()
    {
        for (auto& entry: listeners)
            entry.listener(value);
    }

    void removeListener(int id)
    {
        for (auto it = listeners.begin(); it != listeners.end(); ++it)
        {
            if (it->id == id)
            {
                listeners.erase(it);
                return;
            }
        }
    }

    struct Entry
    {
        int id = 0;
        Listener listener;
    };

    T value;
    EA::Vector<Entry> listeners;
    int nextId = 0;
};

} // namespace eacp
