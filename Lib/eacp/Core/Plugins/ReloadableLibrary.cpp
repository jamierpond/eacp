#include "ReloadableLibrary.h"
#include "DynamicLibraryPlatform.h"
#include "../Utils/Files.h"

#include <atomic>
#include <optional>
#include <string>
#include <utility>

namespace eacp::Plugins
{
namespace
{
int nextStagingSerial()
{
    static auto serial = std::atomic<int> {0};
    return ++serial;
}

// Leaked on purpose, so no image is closed even at static destruction.
Vector<DynamicLibrary>& getImagesKeptMapped()
{
    static auto* images = new Vector<DynamicLibrary> {};
    return *images;
}

std::optional<long long> readStagedCopyProcessId(const std::string& name,
                                                 const FilePath& builtPath)
{
    auto prefix = builtPath.stem() + "-";
    auto extension = builtPath.extension();

    if (!name.starts_with(prefix) || !name.ends_with(extension))
        return {};

    auto fields =
        name.substr(prefix.size(), name.size() - prefix.size() - extension.size());
    auto serialStart = fields.rfind('-');

    if (serialStart == std::string::npos)
        return {};

    auto beforeSerial = fields.substr(0, serialStart);
    auto processStart = beforeSerial.rfind('-');

    if (processStart == std::string::npos)
        return {};

    auto processText = beforeSerial.substr(processStart + 1);

    if (processText.empty()
        || processText.find_first_not_of("0123456789") != std::string::npos)
        return {};

    return std::stoll(processText);
}
} // namespace

ReloadableLibrary::ReloadableLibrary(const FilePath& builtPathToUse,
                                     const FilePath& stagingDirectoryToUse,
                                     const std::string& requiredSymbolToUse)
    : builtPath(builtPathToUse)
    , stagingDirectory(stagingDirectoryToUse)
    , requiredSymbol(requiredSymbolToUse)
{
    removeCopiesOfEndedProcesses();
}

ReloadableLibrary::~ReloadableLibrary()
{
    keepCurrentImageMapped();
}

void ReloadableLibrary::keepCurrentImageMapped()
{
    if (library.isOpen())
        getImagesKeptMapped().add(std::move(library));
}

void ReloadableLibrary::removeCopiesOfEndedProcesses() const
{
    for (const auto& copy: Files::listFiles(stagingDirectory))
    {
        auto processId = readStagedCopyProcessId(copy.fileName(), builtPath);

        if (processId && *processId != Detail::currentProcessId()
            && !Detail::isProcessRunning(*processId))
            Files::removeFile(copy);
    }
}

FilePath ReloadableLibrary::nextStagedPath() const
{
    auto name = builtPath.stem() + "-" + std::to_string(generation + 1) + "-"
                + std::to_string(Detail::currentProcessId()) + "-"
                + std::to_string(nextStagingSerial()) + builtPath.extension();

    return stagingDirectory / name;
}

bool ReloadableLibrary::reload()
{
    error.clear();

    if (!Files::isFile(builtPath))
    {
        error = "no module at " + builtPath.str();
        return false;
    }

    auto staged = nextStagedPath();
    Files::removeFile(staged);

    if (!Files::copyFile(builtPath, staged))
    {
        error = "could not stage " + staged.str();
        return false;
    }

    auto fresh = DynamicLibrary {};

    if (!fresh.open(staged))
    {
        error = fresh.lastError();
        Files::removeFile(staged);
        return false;
    }

    if (!requiredSymbol.empty() && fresh.findSymbol(requiredSymbol) == nullptr)
    {
        error = "no " + requiredSymbol;
        fresh.close();
        Files::removeFile(staged);
        return false;
    }

    keepCurrentImageMapped();
    library = std::move(fresh);
    loadedPath = staged;
    ++generation;
    return true;
}

bool ReloadableLibrary::isOpen() const
{
    return library.isOpen();
}

const std::string& ReloadableLibrary::lastError() const
{
    return error;
}

int ReloadableLibrary::getGeneration() const
{
    return generation;
}

const FilePath& ReloadableLibrary::getBuiltPath() const
{
    return builtPath;
}

const FilePath& ReloadableLibrary::getLoadedPath() const
{
    return loadedPath;
}

void* ReloadableLibrary::findSymbol(const std::string& name) const
{
    return library.findSymbol(name);
}
} // namespace eacp::Plugins
