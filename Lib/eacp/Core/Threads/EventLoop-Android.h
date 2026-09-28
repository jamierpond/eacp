#pragma once

#include "EventLoop-Linux.h"

namespace eacp::Threads
{
// An ALooper event the loop does not own itself: its ident and the data pointer
// it was registered with. android_native_app_glue registers its command pipe
// and input queue this way, and the Graphics layer consumes them.
using LooperEventHandler = std::function<void(int ident, void* data)>;

void setLooperEventHandler(LooperEventHandler handler);
} // namespace eacp::Threads
