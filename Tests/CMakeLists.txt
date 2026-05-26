set(CMAKE_FOLDER Tests)

find_package(NanoTest REQUIRED)

if (APPLE)
    add_subdirectory(ObjC)
endif ()

add_subdirectory(Network)

# Codegen tests exercise eacp's WebView events/hooks format registrations,
# so they only run on the platforms where the WebView library (and its
# eacp-webview-codegen object lib) are built.
if ((APPLE OR WIN32) AND EACP_BUILD_GRAPHICS AND EACP_BUILD_WEBVIEW)
    add_subdirectory(Codegen)
endif ()
