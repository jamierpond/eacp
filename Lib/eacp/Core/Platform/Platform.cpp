#include "Platform.h"
#include "../Plugins/ModuleInfo.h"

namespace eacp::Platform
{

bool isStandalone()
{
    return !isDLL();
}

// An Android app is always a library NativeActivity loads, and it owns the
// loop all the same.
bool isDLL()
{
#if defined(__ANDROID__)
    return false;
#else
    return Plugins::isDynamicLibrary();
#endif
}

} // namespace eacp::Platform
