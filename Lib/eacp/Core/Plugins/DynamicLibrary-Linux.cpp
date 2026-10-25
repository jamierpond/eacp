#include "DynamicLibrary.h"

#include <dlfcn.h>

namespace eacp::Plugins
{
std::string getExportedSymbolName(const void* address)
{
    auto info = Dl_info {};

    if (dladdr(address, &info) == 0 || info.dli_sname == nullptr
        || info.dli_saddr != address)
        return {};

    return info.dli_sname;
}

// No ELF symbol walk yet — enumeration is Mach-O-only for now (see
// DynamicLibrary-Apple.cpp); findSymbol with a known name still works.
Vector<std::string> DynamicLibrary::getFunctionNames() const
{
    return {};
}
} // namespace eacp::Plugins
