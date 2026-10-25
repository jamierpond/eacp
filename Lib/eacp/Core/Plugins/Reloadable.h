#pragma once

#include "../Utils/Console.h"
#include "HotReload.h"
#include "HotReloadLoop.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <type_traits>

// Marks a method of a Plugins::Reloadable<T> struct as reloaded: virtual in a
// hot-reload build, so calls through a pointer or reference land in the latest
// module, and nothing otherwise.
#if EACP_HOT_RELOAD
#define EACP_RELOADABLE virtual
#else
#define EACP_RELOADABLE
#endif

namespace eacp::Plugins
{
using ReloadThunk = void (*)(void* object);

// Every live Reloadable<T> in the process. Main thread only.
namespace ReloadRegistry
{
void registerInstance(void* object, std::size_t objectSize, ReloadThunk thunk);
void unregisterInstance(void* object);

// Instances registered from now on stay on the app's own code.
void forgetLoadedLibrary(const ReloadableLibrary& library);

using FunctionBinder = std::function<void(ReloadableLibrary& library)>;

// Runs `binder` on every reload, and at once when a generation is already
// loaded. The id unregisters it.
int registerFunctionBinder(FunctionBinder binder);
void unregisterFunctionBinder(int binderId);
void rebindFunctionsTo(ReloadableLibrary& fresh);

// Points every registered instance's vtable pointer at the same vtable in
// `fresh`, then calls each one's onReload(). Instances registered later are
// moved to `fresh` as they register, until the next swap or
// forgetLoadedLibrary. Aborts, naming the type, when an
// instance cannot be swapped safely.
void swapVtablesTo(ReloadableLibrary& fresh);
} // namespace ReloadRegistry

// The reload loop for the module eacp_add_hot_reload builds, made from the
// app's hotReloadTarget(): each reload rebinds every ReloadableFunction, then
// swaps every Reloadable, then calls onReloaded. Outside a hot-reload build the
// target names no module, so it holds no loop and nothing reloads.
class HotReloadedModule
{
public:
    explicit HotReloadedModule(const HotReloadTarget& target);
    ~HotReloadedModule();

    HotReloadedModule(const HotReloadedModule&) = delete;
    HotReloadedModule& operator=(const HotReloadedModule&) = delete;

    [[nodiscard]] std::string describeState() const;

    std::function<void()> onReloaded = [] {};
    std::function<void(const std::string& state)> onStateChanged =
        [](const std::string& state) { Console::print(state); };

private:
    void rebindSymbols(ReloadableLibrary& library);

    std::optional<HotReloadLoop> loop;
};

// Holds a T whose EACP_RELOADABLE methods run the latest module's code. Reach
// it only through ->, * or get(): a call on a named object binds straight to
// the app's copy, even at -O0. Each reload, and on construction once a module
// has loaded, the held T's vtable pointer moves to the fresh image and its
// onReload(), if it has one, runs. T keeps its memory, so only function bodies
// may change while the app runs. T needs external linkage, must not be final
// and may have one polymorphic base, listed first. Just the T when hot reload
// is off.
template <typename T>
class Reloadable
{
public:
    template <typename... Args>
        requires(!(std::is_same_v<std::remove_cvref_t<Args>, Reloadable> || ...))
    explicit Reloadable(Args&&... args)
        : value(std::forward<Args>(args)...)
    {
        registerValue();
    }

    Reloadable(const Reloadable& other)
        : value(other.value)
    {
        registerValue();
    }

    Reloadable(Reloadable&& other)
        : value(std::move(other.value))
    {
        registerValue();
    }

    Reloadable& operator=(const Reloadable& other)
    {
        value = other.value;
        return *this;
    }

    Reloadable&
        operator=(Reloadable&& other) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        value = std::move(other.value);
        return *this;
    }

    ~Reloadable()
    {
        if constexpr (isHotReload())
            ReloadRegistry::unregisterInstance(&value);
    }

    T* operator->() { return get(); }
    const T* operator->() const { return get(); }
    T& operator*() { return *get(); }
    const T& operator*() const { return *get(); }
    T* get() { return &value; }
    const T* get() const { return &value; }

private:
    void registerValue()
    {
        if constexpr (isHotReload())
        {
            static_assert(std::is_polymorphic_v<T>,
                          "a reloadable struct needs at least one "
                          "EACP_RELOADABLE method");
            static_assert(!std::is_final_v<T>,
                          "a final struct's calls bypass its vtable, so never "
                          "reload");
            ReloadRegistry::registerInstance(&value, sizeof(T), &callOnReload);
        }
    }

    static void callOnReload(void* object)
    {
        if constexpr (requires(T& reloaded) { reloaded.onReload(); })
            static_cast<T*>(object)->onReload();
    }

    T value;
};

#if !EACP_HOT_RELOAD
namespace Detail
{
struct ReloadableSizeCheck
{
    double payload = 0.0;
};

static_assert(sizeof(Reloadable<ReloadableSizeCheck>)
              == sizeof(ReloadableSizeCheck));
} // namespace Detail
#endif

// A plain function, called through the latest generation of the module, or
// through the copy compiled into the app until one loads. A direct call, and
// no data, when hot reload is off.
template <auto compiledIn>
class ReloadableFunction
{
public:
    ReloadableFunction()
    {
        if constexpr (isHotReload())
        {
            binding.current = compiledIn;
            binding.binderId = ReloadRegistry::registerFunctionBinder(
                [this](ReloadableLibrary& library) { bindTo(library); });
        }
    }

    ReloadableFunction(const ReloadableFunction&) = delete;
    ReloadableFunction& operator=(const ReloadableFunction&) = delete;
    ReloadableFunction(ReloadableFunction&&) = delete;
    ReloadableFunction& operator=(ReloadableFunction&&) = delete;

    ~ReloadableFunction()
    {
        if constexpr (isHotReload())
            ReloadRegistry::unregisterFunctionBinder(binding.binderId);
    }

    template <typename... Args>
    decltype(auto) operator()(Args&&... args) const
    {
        if constexpr (isHotReload())
            return binding.current(std::forward<Args>(args)...);
        else
            return compiledIn(std::forward<Args>(args)...);
    }

private:
    using Function = decltype(compiledIn);

    struct HotBinding
    {
        Function current = compiledIn;
        int binderId = 0;
    };

    struct NoBinding
    {
    };

    void bindTo(ReloadableLibrary& library)
    {
        auto loaded = library.findFunctionMatching(compiledIn);

        if (loaded == nullptr)
            Console::print("hot reload: function not in the module, using the "
                           "compiled-in copy");

        binding.current = loaded != nullptr ? loaded : compiledIn;
    }

    [[no_unique_address]] std::conditional_t<isHotReload(), HotBinding, NoBinding>
        binding;
};
} // namespace eacp::Plugins
