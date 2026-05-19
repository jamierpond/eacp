#pragma once

#include "../../Core/Utils/StateValue.h"

#include <Miro/Miro.h>

#include <string>
#include <utility>

namespace eacp::Graphics
{

// Wires a StateValue<T> into a Miro::Bridge: every set/modify on the
// state broadcasts the new value as an event with the supplied name.
// The returned Subscription owns the binding — destroy it to detach.
//
// The state holds the source of truth; the bridge is just a fan-out.
// C++ subsystems that need to react can attach their own listeners
// via state.addListener() in parallel.
template <typename T>
typename eacp::StateValue<T>::Subscription
bindToBridge(eacp::StateValue<T>& state,
             Miro::Bridge& bridge,
             std::string eventName)
{
    return state.addListener(
        [&bridge, name = std::move(eventName)](const T& value)
        { bridge.emit(name, value); });
}

} // namespace eacp::Graphics
