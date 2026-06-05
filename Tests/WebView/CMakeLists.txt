nano_add_executable(StreamingRangeTests
        SOURCES StreamingRangeTests.cpp
        TARGETS eacp-webview)

nano_add_executable(FileProviderTests
        SOURCES FileProviderTests.cpp
        TARGETS eacp-webview)

nano_add_executable(AsyncBridgeTests
        SOURCES AsyncBridgeTests.cpp
        TARGETS eacp-webview)

# Drives the live streaming pump on a real WebView, so it needs the
# NSApplication / runloop bootstrap from eacp-webview-test-main rather than
# nano_add_executable's bare main(). That target only exists on the platforms
# where the in-process WebView test harness is built (Apple-not-iOS, Windows).
if (TARGET eacp-webview-test-main)
    add_executable(StreamingPumpTests StreamingPumpTests.cpp)
    target_link_libraries(StreamingPumpTests PRIVATE
            eacp-webview
            eacp-webview-test-main)
    nano_discover_tests(StreamingPumpTests)

    # Drives the C++ -> page call path on a real WebView, so it needs the
    # same NSApplication / runloop bootstrap as StreamingPumpTests.
    add_executable(WebViewCallTests WebViewCallTests.cpp)
    target_link_libraries(WebViewCallTests PRIVATE
            eacp-webview
            eacp-webview-test-main)
    nano_discover_tests(WebViewCallTests)

    # Drives page -> C++ command dispatch on a real WebView: per-command
    # execution mode (main vs worker thread) and async Completer handlers.
    add_executable(CommandDispatchTests CommandDispatchTests.cpp)
    target_link_libraries(CommandDispatchTests PRIVATE
            eacp-webview
            eacp-webview-test-main)
    nano_discover_tests(CommandDispatchTests)

    # Asserts the injected window-drag.js resolves drag / no-drag regions on a
    # real WKWebView (same NSApplication / runloop bootstrap as the others).
    # macOS-only: window dragging is installed by WebView.mm's initNative under
    # TARGET_OS_OSX, so the resolver script (and the regression's NSException) only
    # exist on Apple. The Windows WebView2 backend has no window-drag yet, so the
    # script is never injected there and this test would have nothing to drive.
    if (APPLE AND NOT IOS)
        add_executable(WindowDragTests WindowDragTests.cpp)
        target_link_libraries(WindowDragTests PRIVATE
                eacp-webview
                eacp-webview-test-main)
        nano_discover_tests(WindowDragTests)
    endif ()
endif ()
