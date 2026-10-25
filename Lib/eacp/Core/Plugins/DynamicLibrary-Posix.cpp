#include "DynamicLibrary.h"
#include "DynamicLibraryPlatform.h"

#include <cerrno>
#include <cstdint>
#include <csignal>

#include <dlfcn.h>
#include <unistd.h>

namespace eacp::Plugins::Detail
{
void* loadImage(const FilePath& path)
{
    return dlopen(path.c_str(), RTLD_LOCAL | RTLD_NOW);
}

std::string lastLoadError()
{
    auto* message = dlerror();
    return message == nullptr ? std::string {} : std::string {message};
}

long long currentProcessId()
{
    return (long long) getpid();
}

bool isProcessRunning(long long processId)
{
    return kill((pid_t) processId, 0) == 0 || errno != ESRCH;
}

void unloadImage(void* handle)
{
    dlclose(handle);
}

void* findImageSymbol(void* handle, const std::string& name)
{
    return dlsym(handle, name.c_str());
}
} // namespace eacp::Plugins::Detail

namespace eacp::Plugins
{
EnclosingSymbol getEnclosingSymbol(const void* address)
{
    auto info = Dl_info {};

    if (dladdr(address, &info) == 0 || info.dli_sname == nullptr)
        return {};

    auto start = reinterpret_cast<std::uintptr_t>(info.dli_saddr);
    auto at = reinterpret_cast<std::uintptr_t>(address);
    return {info.dli_sname, static_cast<std::size_t>(at - start)};
}
} // namespace eacp::Plugins
