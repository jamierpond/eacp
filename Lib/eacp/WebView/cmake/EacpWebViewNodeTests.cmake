# eacp_add_webview_node_tests — wire a Node/Playwright test suite to a
# WebView test-host executable as a single CMake target.
#
# Usage:
#   eacp_add_webview_node_tests(<TARGET>
#       TEST_HOST  <test-host-executable-target>   # e.g. WebViewTodoTestHost
#       TEST_DIR   <abs path to *.spec.ts dir>     # app-owned tests
#       [PACKAGE_MANAGER  npm|pnpm|yarn|bun]       # default: ${EACP_WEBVIEW_PACKAGE_MANAGER}
#       [FRAMEWORK_DIR    <path>]                  # default: ${EACP_WEBVIEW_NODE_TEST_FRAMEWORK_DIR}
#   )
#
# Produces a single custom target ${TARGET}. Building it (e.g.
# `cmake --build build --target WebViewTodoTests`) rebuilds the
# test-host transitively, then runs Playwright against TEST_DIR.
#
# Communication with the test code happens via two env vars set by the
# generated command line:
#   EACP_TEST_HOST_BINARY  — absolute path to the test-host Mach-O.
#                            Spec files should read this off process.env
#                            and pass it as `bundle:` to launchApp().
#   EACP_PW_TEST_DIR       — absolute path to the spec directory.
#                            playwright.config.ts in the framework
#                            picks it up to override its default
#                            testDir, so a single shared config can
#                            serve every app's tests.

set(EACP_WEBVIEW_NODE_TEST_FRAMEWORK_DIR
        "${CMAKE_SOURCE_DIR}/tools/eacp-test-node"
        CACHE PATH
        "Location of the shared eacp-test-node framework (package.json + src/index.ts).")

function(eacp_add_webview_node_tests TARGET)
    cmake_parse_arguments(PARSE_ARGV 1 ARG ""
            "TEST_HOST;TEST_DIR;PACKAGE_MANAGER;FRAMEWORK_DIR" "")

    if (NOT ARG_TEST_HOST)
        message(FATAL_ERROR "eacp_add_webview_node_tests(${TARGET}): TEST_HOST is required")
    endif ()
    if (NOT TARGET ${ARG_TEST_HOST})
        message(FATAL_ERROR
                "eacp_add_webview_node_tests(${TARGET}): TEST_HOST '${ARG_TEST_HOST}' "
                "is not a target. Declare the test host (e.g. via "
                "TEST_HOST_SOURCES on eacp_add_webview_app) before this call.")
    endif ()
    if (NOT ARG_TEST_DIR)
        message(FATAL_ERROR "eacp_add_webview_node_tests(${TARGET}): TEST_DIR is required")
    endif ()
    if (NOT IS_DIRECTORY "${ARG_TEST_DIR}")
        message(FATAL_ERROR
                "eacp_add_webview_node_tests(${TARGET}): TEST_DIR '${ARG_TEST_DIR}' "
                "does not exist or is not a directory")
    endif ()

    if (NOT ARG_FRAMEWORK_DIR)
        set(ARG_FRAMEWORK_DIR "${EACP_WEBVIEW_NODE_TEST_FRAMEWORK_DIR}")
    endif ()
    if (NOT EXISTS "${ARG_FRAMEWORK_DIR}/package.json")
        message(FATAL_ERROR
                "eacp_add_webview_node_tests(${TARGET}): no package.json found at "
                "FRAMEWORK_DIR='${ARG_FRAMEWORK_DIR}'. Set "
                "EACP_WEBVIEW_NODE_TEST_FRAMEWORK_DIR or pass FRAMEWORK_DIR.")
    endif ()

    if (NOT ARG_PACKAGE_MANAGER)
        if (DEFINED EACP_WEBVIEW_PACKAGE_MANAGER)
            set(ARG_PACKAGE_MANAGER "${EACP_WEBVIEW_PACKAGE_MANAGER}")
        else ()
            set(ARG_PACKAGE_MANAGER "npm")
        endif ()
    endif ()

    find_program(EACP_NODE_TESTS_PM_${ARG_PACKAGE_MANAGER}
            NAMES ${ARG_PACKAGE_MANAGER}.cmd ${ARG_PACKAGE_MANAGER}
            REQUIRED)
    set(PM_EXECUTABLE "${EACP_NODE_TESTS_PM_${ARG_PACKAGE_MANAGER}}")

    # Lazy install at configure time. Mirrors the eacp_webview_add_vite
    # convention so first-build doesn't need a manual `npm install`.
    # Re-running configure after dependency churn re-installs because
    # node_modules will have been wiped.
    if (NOT EXISTS "${ARG_FRAMEWORK_DIR}/node_modules")
        message(STATUS
                "eacp_add_webview_node_tests(${TARGET}): running "
                "${ARG_PACKAGE_MANAGER} install in ${ARG_FRAMEWORK_DIR}")
        execute_process(
                COMMAND ${PM_EXECUTABLE} install
                WORKING_DIRECTORY "${ARG_FRAMEWORK_DIR}"
                RESULT_VARIABLE PM_RESULT)
        if (NOT PM_RESULT EQUAL 0)
            message(FATAL_ERROR
                    "${ARG_PACKAGE_MANAGER} install failed for ${ARG_FRAMEWORK_DIR}")
        endif ()
    endif ()

    # USES_TERMINAL keeps the Playwright "list" reporter rendering
    # incrementally (Ninja otherwise buffers the output until the
    # target finishes). It also serializes against concurrent
    # ninja jobs — handy because a parallel Playwright run is already
    # fully parallel internally.
    add_custom_target(${TARGET}
            COMMENT "Running Node tests for ${ARG_TEST_HOST}"
            COMMAND ${CMAKE_COMMAND} -E env
                    EACP_TEST_HOST_BINARY=$<TARGET_FILE:${ARG_TEST_HOST}>
                    EACP_PW_TEST_DIR=${ARG_TEST_DIR}
                    ${PM_EXECUTABLE} test
            WORKING_DIRECTORY "${ARG_FRAMEWORK_DIR}"
            DEPENDS ${ARG_TEST_HOST}
            USES_TERMINAL
            VERBATIM)
endfunction()
