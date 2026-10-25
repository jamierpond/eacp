#pragma once

// Marks a C function as exported from an eacp plugin. The host resolves
// them with DynamicLibrary::findFunction after enumerating via
// getFunctionNames.
//
// The set of exported functions — and, when host and plugin ship
// independently, their versioning — is the app's own contract. eacp never
// crosses the boundary: host and plugin each statically link their own copy,
// so nothing with C++ layout may pass through these functions; stick to C
// primitives, C structs and function pointers.
//
// A hot-reload module (eacp_add_hot_reload) needs none of this: it links
// nothing and binds at load time to the eacp the host exports, so C++ types
// cross freely, and its functions are plain C++. The host finds each one in a
// fresh image by the name the loader gives its own compiled-in copy
// (ReloadableLibrary::findFunctionMatching).

#ifdef __cplusplus
#define EACP_PLUGIN_EXTERN_C extern "C"
#else
#define EACP_PLUGIN_EXTERN_C
#endif

#ifdef _WIN32
#define EACP_PLUGIN_EXPORT EACP_PLUGIN_EXTERN_C __declspec(dllexport)
#else
#define EACP_PLUGIN_EXPORT                                                          \
    EACP_PLUGIN_EXTERN_C __attribute__((visibility("default")))
#endif
