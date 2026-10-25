#include "Reloadable.h"

#include <cstdint>
#include <cstdlib>
#include <map>

#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#endif

namespace eacp::Plugins
{
namespace
{
struct RegisteredInstance
{
    void* object = nullptr;
    std::string vtableSymbol;
    std::size_t vtableOffset = 0;
    ReloadThunk thunk = nullptr;
    bool isRegistered = true;
    bool isLeftOnAppCode = false;
};

struct VtableSwap
{
    std::size_t index = 0;
    void* freshVtable = nullptr;
};

struct RegisteredFunctionBinder
{
    int id = 0;
    ReloadRegistry::FunctionBinder bind;
};

struct Registry
{
    Vector<RegisteredFunctionBinder> functionBinders;
    int nextFunctionBinderId = 0;
    Vector<RegisteredInstance> instances;
    std::map<std::string, bool> typesCheckedForOneVtablePointer;
    ReloadableLibrary* loadedLibrary = nullptr;
    bool isSwapping = false;
};

constexpr auto vtablePrefix = std::string_view {"_ZTV"};

Registry& getRegistry()
{
    static auto registry = Registry {};
    return registry;
}

std::string getTypeNameFromVtableSymbol(const std::string& symbol)
{
    auto mangledType = "_Z" + symbol.substr(vtablePrefix.size());

#if __has_include(<cxxabi.h>)
    auto status = 0;
    auto* demangled =
        abi::__cxa_demangle(mangledType.c_str(), nullptr, nullptr, &status);

    if (status == 0 && demangled != nullptr)
    {
        auto name = std::string {demangled};
        std::free(demangled);
        return name;
    }
#endif

    return symbol;
}

[[noreturn]] void abortReload(const std::string& message)
{
    Console::printError("hot reload: " + message);
    std::abort();
}

bool isVtableSymbol(const std::string& symbol)
{
    return symbol.starts_with(vtablePrefix);
}

void abortIfSecondVtablePointer(const void* object,
                                std::size_t objectSize,
                                const std::string& vtableSymbol)
{
    auto* words = static_cast<void* const*>(object);
    auto numWords = objectSize / sizeof(void*);

    for (auto index = std::size_t {1}; index < numWords; ++index)
    {
        if (getEnclosingSymbol(words[index]).name != vtableSymbol)
            continue;

        abortReload(getTypeNameFromVtableSymbol(vtableSymbol)
                    + " has a second vtable pointer, at byte "
                    + std::to_string(index * sizeof(void*))
                    + ", which a reload cannot swap. Give it at most one "
                      "polymorphic base, listed first, and restart the app.");
    }
}

EnclosingSymbol findVtableOf(const void* object, std::size_t objectSize)
{
    auto* vtablePointer = *static_cast<void* const*>(object);
    auto vtable = getEnclosingSymbol(vtablePointer);

    if (!isVtableSymbol(vtable.name))
        abortReload("an instance at "
                    + std::to_string(reinterpret_cast<std::uintptr_t>(object))
                    + " has no exported vtable; a reloadable struct needs "
                      "external linkage and an EACP_RELOADABLE method. Fix "
                      "it and restart the app.");

    auto& checked = getRegistry().typesCheckedForOneVtablePointer;

    if (!checked.contains(vtable.name))
    {
        abortIfSecondVtablePointer(object, objectSize, vtable.name);
        checked[vtable.name] = true;
    }

    return vtable;
}

void* findFreshVtable(const RegisteredInstance& instance, ReloadableLibrary& fresh)
{
    auto* group = static_cast<char*>(fresh.findSymbol(instance.vtableSymbol));

    if (group == nullptr)
        return nullptr;

    return group + instance.vtableOffset;
}

void writeVtablePointer(void* object, void* vtable)
{
    *static_cast<void**>(object) = vtable;
}

void removeUnregisteredInstances()
{
    getRegistry().instances.removeIndexesMatching(
        [](const RegisteredInstance& instance) { return !instance.isRegistered; });
}
} // namespace

namespace ReloadRegistry
{
void registerInstance(void* object, std::size_t objectSize, ReloadThunk thunk)
{
    auto vtable = findVtableOf(object, objectSize);
    auto instance = RegisteredInstance {object, vtable.name, vtable.offset, thunk};

    if (auto* loaded = getRegistry().loadedLibrary)
    {
        if (auto* fresh = findFreshVtable(instance, *loaded))
        {
            writeVtablePointer(object, fresh);
            thunk(object);
        }
        else
        {
            instance.isLeftOnAppCode = true;
            Console::printError(
                "hot reload: no vtable for "
                + getTypeNameFromVtableSymbol(vtable.name)
                + " in the loaded module, so this instance runs the app's own "
                  "code; its key function is not in a reloaded source file. "
                  "Add that file to SOURCES to reload it.");
        }
    }

    getRegistry().instances.add(std::move(instance));
}

int registerFunctionBinder(FunctionBinder binder)
{
    auto& registry = getRegistry();
    auto id = ++registry.nextFunctionBinderId;

    if (auto* loaded = registry.loadedLibrary)
        binder(*loaded);

    registry.functionBinders.add({id, std::move(binder)});
    return id;
}

void unregisterFunctionBinder(int binderId)
{
    getRegistry().functionBinders.removeIndexesMatching(
        [binderId](const RegisteredFunctionBinder& binder)
        { return binder.id == binderId; });
}

void rebindFunctionsTo(ReloadableLibrary& fresh)
{
    for (auto& binder: getRegistry().functionBinders)
        binder.bind(fresh);
}

void forgetLoadedLibrary(const ReloadableLibrary& library)
{
    if (getRegistry().loadedLibrary == &library)
        getRegistry().loadedLibrary = nullptr;
}

void unregisterInstance(void* object)
{
    auto& registry = getRegistry();

    for (auto& instance: registry.instances)
        if (instance.object == object)
            instance.isRegistered = false;

    if (!registry.isSwapping)
        removeUnregisteredInstances();
}

void swapVtablesTo(ReloadableLibrary& fresh)
{
    auto& registry = getRegistry();
    auto numInstances = registry.instances.getSize();
    auto swaps = Vector<VtableSwap> {};

    for (auto index = std::size_t {0}; index < numInstances; ++index)
    {
        auto& instance = registry.instances[index];

        if (instance.isLeftOnAppCode || !instance.isRegistered)
            continue;

        auto* freshVtable = findFreshVtable(instance, fresh);

        if (freshVtable == nullptr)
            abortReload("no vtable for "
                        + getTypeNameFromVtableSymbol(instance.vtableSymbol)
                        + " in the new module; its key function is not in a "
                          "reloaded source file. Add that file to SOURCES or "
                          "restart the app.");

        swaps.add({index, freshVtable});
    }

    for (auto& swap: swaps)
        writeVtablePointer(registry.instances[swap.index].object, swap.freshVtable);

    registry.loadedLibrary = &fresh;

    if (!swaps.empty())
        Console::print("hot reload: swapped " + std::to_string(swaps.size())
                       + " instance vtables to generation "
                       + std::to_string(fresh.getGeneration()));

    registry.isSwapping = true;

    for (auto& swap: swaps)
    {
        auto& instance = registry.instances[swap.index];

        if (!instance.isRegistered)
            continue;

        auto* object = instance.object;
        auto thunk = instance.thunk;
        thunk(object);
    }

    registry.isSwapping = false;
    removeUnregisteredInstances();
}
} // namespace ReloadRegistry

HotReloadedModule::HotReloadedModule(const HotReloadTarget& target)
{
    if (target.library.empty())
        return;

    auto& started = loop.emplace(target);
    started.onStateChanged = [this] { onStateChanged(loop->describeState()); };
    started.onReloaded = [this](ReloadableLibrary& library)
    { rebindSymbols(library); };
}

HotReloadedModule::~HotReloadedModule()
{
    if (loop)
        ReloadRegistry::forgetLoadedLibrary(loop->getLibrary());
}

std::string HotReloadedModule::describeState() const
{
    return loop ? loop->describeState() : "hot reload off";
}

void HotReloadedModule::rebindSymbols(ReloadableLibrary& library)
{
    ReloadRegistry::rebindFunctionsTo(library);
    ReloadRegistry::swapVtablesTo(library);
    onReloaded();
}
} // namespace eacp::Plugins
