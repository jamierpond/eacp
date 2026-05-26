# eacp-webview-test
#
# Optional helper library that mounts a "test.*" command surface on a
# Miro::Bridge for driving a WebView from outside the process — pair it
# with eacp-network-rpc so an external test runner can POST against
# /rpc.
#
# Intentionally a separate target from eacp-webview: shipping apps link
# eacp-webview only; test-host targets link eacp-webview-test
# additionally. That way nothing test-related winds up in the binaries
# you actually ship.

add_library(eacp-webview-test STATIC)

target_sources(eacp-webview-test PRIVATE TestHarness.cpp)

target_link_libraries(eacp-webview-test PUBLIC eacp-webview)

set_default_target_setting(eacp-webview-test)
eacp_enable_unity_build(eacp-webview-test)
add_ide_sources(eacp-webview-test)
