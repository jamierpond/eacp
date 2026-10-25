#pragma once

#include "../Process/BuildRunner.h"
#include "../Utils/SourceWatcher.h"
#include "HotReloadTarget.h"
#include "ReloadableLibrary.h"

#include <map>
#include <type_traits>

namespace eacp::Plugins
{
struct BuildFailure
{
    std::string output;
    bool launched = false;
    bool exited = false;
    int exitCode = -1;

    // The line most likely to name the cause: a located compiler error, then
    // any error that is not the driver's, a CMake error, ninja's FAILED: line,
    // and the last line of output as the fallback.
    std::string summary;
};

BuildFailure buildFailureFrom(const Processes::ProcessResult& result);

// Watches a source tree and, on each settled burst of changes, runs the build
// command and reloads the module it produces. A failed build or load keeps the
// current generation. Nothing builds until the first change. Main thread only;
// every callback runs on the main thread and must not destroy the loop.
//
// Built from a HotReloadTarget, the loop refuses any image that lacks the
// host's own requiredHostFunction, found by the name the loader gives the
// host's copy, so no name is ever spelled. Without one, every image is taken.
//
// A header under the source directory whose contents differ from launch may
// have changed a struct the running app laid out, so while one differs nothing
// builds or reloads, rebuild() included, and the state stays HeaderChanged;
// a build already running finishes unused. Undo the edit or restart.
class HotReloadLoop
{
public:
    enum class State
    {
        Idle,
        Building,
        Loaded,
        BuildFailed,
        LoadFailed,
        HeaderChanged
    };

    struct Options
    {
        Files::SourceWatcher::Options sources;
        Processes::ProcessOptions buildCommand;
        FilePath modulePath;
        FilePath stagingDirectory = FilePath::appCacheDirectory() / "HotReload";
        std::string requiredSymbol;
    };

    explicit HotReloadLoop(const Options& options);
    explicit HotReloadLoop(const HotReloadTarget& target,
                           const void* requiredHostFunction = nullptr);

    template <typename Function>
        requires std::is_function_v<std::remove_pointer_t<Function>>
    HotReloadLoop(const HotReloadTarget& target, Function requiredHostFunction)
        : HotReloadLoop(target, reinterpret_cast<const void*>(requiredHostFunction))
    {
    }

    HotReloadLoop(const HotReloadLoop&) = delete;
    HotReloadLoop& operator=(const HotReloadLoop&) = delete;
    HotReloadLoop(HotReloadLoop&&) = delete;
    HotReloadLoop& operator=(HotReloadLoop&&) = delete;

    void rebuild();

    [[nodiscard]] State getState() const;
    [[nodiscard]] std::string describeState() const;
    [[nodiscard]] int getGeneration() const;
    [[nodiscard]] ReloadableLibrary& getLibrary();
    [[nodiscard]] const FilePath& getSourceDirectory() const;

    std::function<void()> onBuildStarted = [] {};
    std::function<void(std::string_view chunk)> onBuildOutput =
        [](std::string_view) {};
    std::function<void(ReloadableLibrary& library)> onReloaded =
        [](ReloadableLibrary&) {};
    std::function<void(const BuildFailure& failure)> onBuildFailed =
        [](const BuildFailure&) {};
    std::function<void(const std::string& error)> onLoadFailed =
        [](const std::string&) {};
    std::function<void()> onStateChanged = [] {};

private:
    Files::FileWatcher::ChangedFunc rebuildOnChange();
    void rebuildUnlessAHeaderChanged(const Vector<FilePath>& changed);
    [[nodiscard]] bool hasHeaderChangedSinceLaunch(const FilePath& header) const;
    [[nodiscard]] bool hasAnyHeaderChanged() const;
    void startBuilding();
    void buildFinished(const Processes::ProcessResult& result);
    void reload();
    void setState(State newState);

    ReloadableLibrary library;
    Processes::BuildRunner runner;
    State state = State::Idle;
    std::string failureMessage;
    std::map<std::string, std::size_t> headerContentsAtLaunch;
    Vector<FilePath> headersChangedSinceLaunch;
    Files::SourceWatcher watcher;
};
} // namespace eacp::Plugins
