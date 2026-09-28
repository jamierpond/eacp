#include "EventLoop-Android.h"
#include "ThreadUtils-Linux.h"
#include "../Utils/Logging.h"
#include "../Utils/Singleton.h"

#include <android/looper.h>

#include <atomic>
#include <fcntl.h>
#include <mutex>
#include <poll.h>
#include <unistd.h>

// EventLoop-Linux.cpp's loop over ALooper instead of poll(2): what Android
// delivers (the glue's commands, the input queue, choreographer frames) is
// registered with the thread's looper, not exposed as descriptors.

namespace eacp::Threads
{
namespace
{
constexpr int wakerIdent = 1000;
constexpr int firstSourceIdent = 1001;

struct AndroidPipeWaker
{
    AndroidPipeWaker()
    {
        int fds[2];
        if (::pipe(fds) != 0)
            return;

        readFd = fds[0];
        writeFd = fds[1];

        ::fcntl(readFd, F_SETFL, O_NONBLOCK);
        ::fcntl(writeFd, F_SETFL, O_NONBLOCK);
        ::fcntl(readFd, F_SETFD, FD_CLOEXEC);
        ::fcntl(writeFd, F_SETFD, FD_CLOEXEC);
    }

    ~AndroidPipeWaker()
    {
        if (readFd >= 0)
            ::close(readFd);
        if (writeFd >= 0)
            ::close(writeFd);
    }

    void wake()
    {
        char b = 1;
        auto r = ::write(writeFd, &b, 1);
        (void) r;
    }

    void drain()
    {
        char buf[64];
        while (::read(readFd, buf, sizeof(buf)) > 0)
        {
        }
    }

    int readFd = -1;
    int writeFd = -1;
};

struct AndroidLoopSource
{
    int fd = -1;
    int ident = 0;
    short events = 0;
    Callback callback;
    Callback prepare;
};

struct AndroidLoopState
{
    AndroidPipeWaker waker;
    std::mutex mutex;
    Vector<Callback> queue;
    std::atomic<bool> running {false};

    std::mutex sourceMutex;
    Vector<AndroidLoopSource> sources;
    int nextIdent = firstSourceIdent;

    ALooper* looper = nullptr;
    LooperEventHandler looperHandler = [](int, void*) {};
};

AndroidLoopState& getAndroidLoop()
{
    return Singleton::get<AndroidLoopState>();
}

int looperEventsFor(short events)
{
    auto result = 0;

    if ((events & POLLIN) != 0)
        result |= ALOOPER_EVENT_INPUT;
    if ((events & POLLOUT) != 0)
        result |= ALOOPER_EVENT_OUTPUT;

    return result;
}

void attachSource(ALooper* looper, const AndroidLoopSource& source)
{
    ALooper_addFd(looper,
                  source.fd,
                  source.ident,
                  looperEventsFor(source.events),
                  nullptr,
                  nullptr);
}

void attachLooper(AndroidLoopState& loop)
{
    if (loop.looper != nullptr)
        return;

    loop.looper = ALooper_prepare(0);
    ALooper_acquire(loop.looper);

    ALooper_addFd(loop.looper,
                  loop.waker.readFd,
                  wakerIdent,
                  ALOOPER_EVENT_INPUT,
                  nullptr,
                  nullptr);

    auto lock = std::lock_guard(loop.sourceMutex);

    for (const auto& source: loop.sources)
        attachSource(loop.looper, source);
}

void drainAndroidPending(AndroidLoopState& loop)
{
    auto pending = Vector<Callback>();
    {
        auto lock = std::lock_guard(loop.mutex);
        pending = std::move(loop.queue);
    }
    for (auto& cb: pending)
        cb();
}

void runAndroidSourcePrepares(AndroidLoopState& loop)
{
    auto prepares = Vector<Callback> {};

    {
        auto lock = std::lock_guard(loop.sourceMutex);

        for (const auto& source: loop.sources)
            if (source.prepare)
                prepares.add(source.prepare);
    }

    for (auto& prepare: prepares)
        prepare();
}

void dispatchAndroidSource(AndroidLoopState& loop, int ident)
{
    auto callback = Callback {};

    {
        auto lock = std::lock_guard(loop.sourceMutex);

        for (const auto& source: loop.sources)
            if (source.ident == ident)
                callback = source.callback;
    }

    if (callback)
        callback();
}

// False when the looper gave up (an error, not a timeout).
bool pollAndroidLooper(AndroidLoopState& loop, int timeoutMs, bool& timedOut)
{
    runAndroidSourcePrepares(loop);

    auto data = static_cast<void*>(nullptr);
    auto ident = ALooper_pollOnce(timeoutMs, nullptr, nullptr, &data);

    if (ident == ALOOPER_POLL_TIMEOUT)
    {
        timedOut = true;
        return true;
    }

    if (ident == ALOOPER_POLL_ERROR)
    {
        LOG("EventLoop: ALooper_pollOnce failed");
        return false;
    }

    if (ident == wakerIdent)
        loop.waker.drain();
    else if (ident >= firstSourceIdent)
        dispatchAndroidSource(loop, ident);
    else if (ident >= 0)
        loop.looperHandler(ident, data);

    drainAndroidPending(loop);
    return true;
}
} // namespace

void setLooperEventHandler(LooperEventHandler handler)
{
    getAndroidLoop().looperHandler =
        handler ? std::move(handler) : LooperEventHandler {[](int, void*) {}};
}

void EventLoop::run()
{
    initMainThread();

    auto& loop = getAndroidLoop();
    attachLooper(loop);
    loop.running = true;

    // Work queued before the loop started would otherwise wait for a wake.
    drainAndroidPending(loop);

    while (loop.running)
    {
        auto timedOut = false;

        if (!pollAndroidLooper(loop, -1, timedOut))
            break;
    }
}

bool EventLoop::runFor(Time::MS timeout)
{
    initMainThread();

    auto& loop = getAndroidLoop();
    attachLooper(loop);
    loop.running = true;

    auto deadline = Time::Deadline {timeout};

    while (loop.running)
    {
        if (deadline.expired())
            return false;

        auto timedOut = false;

        if (!pollAndroidLooper(loop, (int) deadline.remaining().count, timedOut))
            break;

        if (timedOut)
            return false;
    }

    return true;
}

void EventLoop::quit()
{
    auto& loop = getAndroidLoop();
    loop.running = false;
    loop.waker.wake();
}

void EventLoop::call(Callback func)
{
    auto& loop = getAndroidLoop();
    {
        auto lock = std::lock_guard(loop.mutex);
        loop.queue.add(std::move(func));
    }
    loop.waker.wake();
}

void addLoopSource(int fd, short events, Callback callback, Callback prepare)
{
    auto& loop = getAndroidLoop();
    auto lock = std::lock_guard(loop.sourceMutex);

    loop.sources.removeIndexesMatching([fd](const AndroidLoopSource& source)
                                       { return source.fd == fd; });

    auto source = AndroidLoopSource {
        fd, loop.nextIdent++, events, std::move(callback), std::move(prepare)};

    if (loop.looper != nullptr)
        attachSource(loop.looper, source);

    loop.sources.add(std::move(source));
}

void addLoopSource(int fd, short events, Callback callback)
{
    addLoopSource(fd, events, std::move(callback), Callback {});
}

void removeLoopSource(int fd)
{
    auto& loop = getAndroidLoop();
    auto lock = std::lock_guard(loop.sourceMutex);

    loop.sources.removeIndexesMatching([fd](const AndroidLoopSource& source)
                                       { return source.fd == fd; });

    if (loop.looper != nullptr)
        ALooper_removeFd(loop.looper, fd);
}

void scheduleStartup(const Callback& func)
{
    callAsync(func);
}

bool isEventLoopRunning()
{
    return getAndroidLoop().running.load();
}

void stopProcessRootLoop() {}

} // namespace eacp::Threads
