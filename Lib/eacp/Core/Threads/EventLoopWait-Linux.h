#pragma once

namespace eacp::Threads
{
enum class WaitResult
{
    Ready,
    TimedOut,
    Failed
};

// Blocks until the loop's epoll descriptor is readable or `timeoutMs` passes
// (-1 waits for ever). Android waits in the thread's ALooper instead, since
// that is where the system delivers its own events.
WaitResult waitForLoopFd(int epollFd, int timeoutMs);
} // namespace eacp::Threads
