#pragma once

#include "DynamicLibrary.h"

namespace eacp::Plugins
{
// A DynamicLibrary over a module file that is rebuilt while the process runs.
//
// reload() opens a fresh copy of builtPath staged under its own name in
// stagingDirectory, never builtPath itself: a new path per load sidesteps
// DynamicLibrary's path-keyed registry (which would hand back the old image)
// and Windows' lock on a loaded DLL, which the linker must be free to
// overwrite.
//
// No image is ever unmapped, not on reload and not on destruction: whatever a
// generation's code made (a std::function, an object with a vtable, a string
// literal) may outlive it, so every image stays mapped until the process
// exits. Their staged copies stay too, and are removed by the first
// ReloadableLibrary on the same staging directory once their process has
// ended. A development-only cost. Main thread only.
//
// With a requiredSymbol, an image that does not export it is refused before
// it replaces the current one, so the previous generation stays loaded.
class ReloadableLibrary
{
public:
    ReloadableLibrary(const FilePath& builtPathToUse,
                      const FilePath& stagingDirectoryToUse,
                      const std::string& requiredSymbolToUse = {});
    ~ReloadableLibrary();

    ReloadableLibrary(const ReloadableLibrary&) = delete;
    ReloadableLibrary& operator=(const ReloadableLibrary&) = delete;
    ReloadableLibrary(ReloadableLibrary&&) = delete;
    ReloadableLibrary& operator=(ReloadableLibrary&&) = delete;

    // Stages a fresh copy of the built path and opens it, handing the previous
    // image to Plugins::unload. False leaves the current image untouched.
    bool reload();

    [[nodiscard]] bool isOpen() const;

    // Why the last reload() failed: the loader's message, or what went wrong
    // staging the copy. Empty after a successful reload().
    [[nodiscard]] const std::string& lastError() const;

    // 0 before the first successful reload, one more per success.
    [[nodiscard]] int getGeneration() const;

    [[nodiscard]] const FilePath& getBuiltPath() const;

    // The staged copy currently open; empty when none is.
    [[nodiscard]] const FilePath& getLoadedPath() const;

    template <typename FunctionType>
    FunctionType findFunction(const std::string& name) const
    {
        return library.findFunction<FunctionType>(name);
    }

    // The fresh image's copy of a function the host has compiled in, found by
    // the name the loader gives the host's copy; nullptr when either image
    // does not export it.
    template <typename FunctionType>
    FunctionType findFunctionMatching(FunctionType hostFunction) const
    {
        auto* address = reinterpret_cast<const void*>(hostFunction);
        auto name = getExportedSymbolName(address);

        if (name.empty())
            return nullptr;

        return findFunction<FunctionType>(name);
    }

    [[nodiscard]] void* findSymbol(const std::string& name) const;

private:
    FilePath nextStagedPath() const;
    void removeCopiesOfEndedProcesses() const;
    void keepCurrentImageMapped();

    FilePath builtPath;
    FilePath stagingDirectory;
    std::string requiredSymbol;
    FilePath loadedPath;
    DynamicLibrary library;
    std::string error;
    int generation = 0;
};
} // namespace eacp::Plugins
