#pragma once

#include <utility>

namespace eacp::Singleton
{
template <typename T>
T& get()
{
    static T object;
    return object;
}

// Constructor-arg overload. Args are forwarded to T's constructor on
// first call; subsequent calls return the same object and ignore the
// args (standard static-local semantics).
template <typename T, typename Arg, typename... Args>
T& get(Arg&& arg, Args&&... args)
{
    static T object(std::forward<Arg>(arg), std::forward<Args>(args)...);
    return object;
}
} // namespace eacp::Singleton