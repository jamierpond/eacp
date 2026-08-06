#include "Instance.h"

namespace eacp::React
{
Instance::~Instance()
{
    // Said first, so anything holding a token to this -- a setter in a callback,
    // an effect waiting for the commit -- finds it gone rather than following a
    // pointer into freed storage.
    if (alive != nullptr)
        *alive = nullptr;

    // Children before this one's own cleanups, which is the order React tears a
    // tree down in and the order the effects were written expecting: a parent's
    // cleanup can assume its children have already stopped.
    children.clear();

    for (auto& hook: hooks)
        hook->unmount();
}
} // namespace eacp::React
