include(FetchContent)

find_package(ResEmbed REQUIRED)
find_package(Miro REQUIRED)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/EacpWebViewVite.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/EacpWebViewBackend.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/EacpWebViewReact.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/EacpWebViewApp.cmake)

add_library(eacp-webview STATIC)

target_link_libraries(eacp-webview PUBLIC eacp-graphics ResEmbed Miro)

# WKUIDelegate's public camera/mic permission selector is macOS 12+. When the
# macOS deployment target is older, opt into WebKit's private _WK SPI so
# getUserMedia still works on macOS 11. iOS uses a different SPI we don't
# implement here, so this is macOS-only.
if (APPLE AND NOT IOS AND CMAKE_OSX_DEPLOYMENT_TARGET
        AND CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "12.0")
    target_compile_definitions(eacp-webview PRIVATE
            EACP_WEBVIEW_PRIVATE_MEDIA_CAPTURE_SPI=1)
endif ()

target_sources(eacp-webview PRIVATE
        WebView/WebView-Shared.cpp
        WebView/DevServerProbe.cpp
        WebView/Bridge.cpp
        WebView/StateBridge.cpp
        WebView/MenuHelpers.cpp)

if (APPLE)
    target_sources(eacp-webview PRIVATE
            WebView/WebView.mm
            WebView/DevServerProbe-Posix.cpp)
    target_link_libraries(eacp-webview PRIVATE
            "-framework Foundation"
            "-framework WebKit")

    if (IOS)
        target_link_libraries(eacp-webview PRIVATE
                "-framework UIKit")
    else ()
        target_link_libraries(eacp-webview PRIVATE
                "-framework AppKit")
    endif ()
elseif (WIN32)
    target_sources(eacp-webview PRIVATE
            WebView/WebView-Windows.cpp
            WebView/DevServerProbe-Windows.cpp)

    find_package(WebView2 REQUIRED)

    target_link_libraries(eacp-webview PRIVATE
            WebView2::WebView2
            ws2_32)
else ()
    target_sources(eacp-webview PRIVATE WebView/DevServerProbe-Posix.cpp)
endif ()

set_default_target_setting(eacp-webview)
eacp_enable_unity_build(eacp-webview)
add_ide_sources(eacp-webview)

# Codegen-only object library — bundles the eacp events / hooks
# formatters and their static-init registration with Miro's format
# registry. Spliced into each app's ${TARGET}Schema_Codegen executable
# by eacp_add_webview_app, so the format registrations only run inside
# the build-time codegen tool, never in the shipping runtime.
#
# Skipped when cross-compiling — symmetric with MiroTypeExportMain.
if (NOT CMAKE_CROSSCOMPILING)
    add_library(eacp-webview-codegen OBJECT
            Codegen/EventsFormat.cpp
            Codegen/HooksFormat.cpp
            Codegen/Register.cpp)
    target_link_libraries(eacp-webview-codegen PUBLIC Miro)
    set_default_target_setting(eacp-webview-codegen)
endif ()
