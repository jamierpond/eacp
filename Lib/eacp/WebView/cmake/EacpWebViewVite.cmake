option(EACP_WEBVIEW_VITE_BUILD "Let CMake drive 'npm install' + 'vite build' for embedded webview apps. When OFF, the prebuilt dist committed at SOURCE_DIR/dist is embedded as-is. NOTE: when ON, the first 'cmake --build' produces the dist but embeds an empty resource registry (configure-time glob saw no files); CONFIGURE_DEPENDS makes the next build invocation reglob and embed for real." ON)

function(eacp_webview_add_vite TARGET)
    cmake_parse_arguments(PARSE_ARGV 1 ARG ""
            "SOURCE_DIR;DIST_DIR;NAMESPACE;CATEGORY" "")

    if (NOT ARG_SOURCE_DIR)
        message(FATAL_ERROR "eacp_webview_add_vite: SOURCE_DIR is required")
    endif ()
    if (NOT ARG_NAMESPACE)
        set(ARG_NAMESPACE "Resources")
    endif ()
    if (NOT ARG_CATEGORY)
        set(ARG_CATEGORY "Resources")
    endif ()
    if (NOT ARG_DIST_DIR)
        set(ARG_DIST_DIR "${ARG_SOURCE_DIR}/dist")
    endif ()

    if (EACP_WEBVIEW_VITE_BUILD AND EXISTS "${ARG_SOURCE_DIR}/package.json")
        find_program(NPM_EXECUTABLE npm REQUIRED)
        set(BUILD_DIST_DIR "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-vite-dist")

        if (NOT EXISTS "${ARG_SOURCE_DIR}/node_modules")
            message(STATUS "eacp_webview_add_vite(${TARGET}): running npm install")
            execute_process(
                    COMMAND ${NPM_EXECUTABLE} install
                    WORKING_DIRECTORY "${ARG_SOURCE_DIR}"
                    RESULT_VARIABLE NPM_INSTALL_RESULT)
            if (NOT NPM_INSTALL_RESULT EQUAL 0)
                message(FATAL_ERROR "npm install failed for ${TARGET}")
            endif ()
        endif ()

        # ResourceGenerator rejects an empty input list, so seed BUILD_DIST_DIR
        # with a placeholder. Vite's --emptyOutDir wipes the dist dir at build
        # time, so the custom_command copies the template back afterwards.
        # Copying (rather than touching) keeps the placeholder non-empty, so
        # the embedded C array isn't a zero-length extension.
        file(MAKE_DIRECTORY "${BUILD_DIST_DIR}")
        set(VITE_PLACEHOLDER_TEMPLATE
                "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-vite-placeholder.txt")
        set(VITE_PLACEHOLDER "${BUILD_DIST_DIR}/placeholder.txt")
        file(WRITE "${VITE_PLACEHOLDER_TEMPLATE}"
                "Vite build placeholder — kept so the embedded resource list is non-empty.\n")
        configure_file("${VITE_PLACEHOLDER_TEMPLATE}" "${VITE_PLACEHOLDER}" COPYONLY)

        file(GLOB_RECURSE VITE_SOURCES CONFIGURE_DEPENDS
                "${ARG_SOURCE_DIR}/src/*"
                "${ARG_SOURCE_DIR}/public/*"
                "${ARG_SOURCE_DIR}/index.html"
                "${ARG_SOURCE_DIR}/package.json"
                "${ARG_SOURCE_DIR}/vite.config.*"
                "${ARG_SOURCE_DIR}/tsconfig*.json")

        set(VITE_STAMP "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-vite.stamp")
        add_custom_command(
                OUTPUT "${VITE_STAMP}"
                COMMAND ${NPM_EXECUTABLE} run build --
                        --outDir "${BUILD_DIST_DIR}" --emptyOutDir
                COMMAND ${CMAKE_COMMAND} -E copy
                        "${VITE_PLACEHOLDER_TEMPLATE}" "${VITE_PLACEHOLDER}"
                COMMAND ${CMAKE_COMMAND} -E touch "${VITE_STAMP}"
                WORKING_DIRECTORY "${ARG_SOURCE_DIR}"
                DEPENDS ${VITE_SOURCES}
                COMMENT "Building Vite project for ${TARGET}"
                VERBATIM)

        add_custom_target(${TARGET}-vite-build DEPENDS "${VITE_STAMP}")
        add_dependencies(${TARGET} ${TARGET}-vite-build)

        res_embed_add(${TARGET}
                DIRECTORY "${BUILD_DIST_DIR}"
                NAMESPACE ${ARG_NAMESPACE}
                CATEGORY ${ARG_CATEGORY})
        return()
    endif ()

    if (NOT EXISTS "${ARG_DIST_DIR}")
        message(FATAL_ERROR
                "eacp_webview_add_vite(${TARGET}): prebuilt Vite dist not found at "
                "${ARG_DIST_DIR}. Either run 'npm run build' in ${ARG_SOURCE_DIR} "
                "and commit the output, or configure with "
                "-DEACP_WEBVIEW_VITE_BUILD=ON to have CMake drive the Vite build.")
    endif ()

    res_embed_add(${TARGET}
            DIRECTORY "${ARG_DIST_DIR}"
            NAMESPACE ${ARG_NAMESPACE}
            CATEGORY ${ARG_CATEGORY})
endfunction()
