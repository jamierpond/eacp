include(AppleSetup)

function(set_default_warnings_level target)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif ()
endfunction()

function(set_default_target_setting target)
    set_default_warnings_level(${target})
    set_target_properties(${target} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    if (IOS)
        set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${EACP_IOS_PLIST}")
    elseif (APPLE)
        set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${EACP_MACOS_PLIST}")
    endif ()
endfunction()

function(eacp_enable_unity_build target)
    if (EACP_UNITY_BUILD)
        set_target_properties(${target} PROPERTIES UNITY_BUILD ON)
    endif ()
endfunction()

# Force a target to compile at the platform's maximum *speed* optimization in
# EVERY configuration -- including Debug -- without enabling fast-math: IEEE FP
# semantics and bit-exact results are preserved (no multiply-add contraction).
# Intended for hot numeric / SIMD kernels that are pointless at -O0 / -Od. Debug
# info (-g / /Zi) is left untouched, so the target stays debuggable, just fast.
#
# Removing cl's Debug-only /RTC1 and /Od edits the *directory-scoped* Debug flags
# (hence PARENT_SCOPE), so keep each force-optimized target in its own
# subdirectory -- the usual case. The rest of the project keeps the defaults.
function(eacp_force_optimization target)
    if (MSVC)
        # The cl/clang-cl Debug defaults fight optimization: /RTC1 is a hard
        # error under any /O level (D8016) and /Od warns when overridden by /O2
        # (D9025). Strip both before forcing the level below.
        string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_CXX_FLAGS_DEBUG
                "${CMAKE_CXX_FLAGS_DEBUG}")
        string(REGEX REPLACE "/Od" "" CMAKE_CXX_FLAGS_DEBUG
                "${CMAKE_CXX_FLAGS_DEBUG}")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}" PARENT_SCOPE)

        # clang-cl reports CXX_COMPILER_ID==Clang but parses the MSVC-style
        # driver, so its GCC-style flags must tunnel through /clang: or they are
        # silently dropped -- and a dropped -ffp-contract=off lets clang-cl fuse
        # FMA, breaking bit-exactness. Real cl has no /O3 (tops out at /O2) and
        # never contracts FP by default (/fp:precise).
        if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target} PRIVATE
                    /clang:-O3 /clang:-ffp-contract=off)
        else ()
            target_compile_options(${target} PRIVATE /O2)
        endif ()
    else ()
        target_compile_options(${target} PRIVATE -O3 -ffp-contract=off)
    endif ()
endfunction()

# Mark an executable as a windowed GUI app so launching it never pops a console
# window on Windows — the cross-platform analogue of MACOSX_BUNDLE on Apple. A
# no-op everywhere but Windows. Call this on any eacp app that owns a window
# (graphics/tray/webview apps) rather than a console.
function(eacp_set_gui_subsystem target)
    set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE TRUE)
    # WIN32_EXECUTABLE selects /SUBSYSTEM:WINDOWS, whose default MSVC entry point
    # is WinMainCRTStartup (it expects WinMain). eacp apps use a standard
    # int main(), so redirect the entry to the console CRT startup — it still
    # parses argv and calls main(), just without a console attached.
    if (MSVC)
        target_link_options(${target} PRIVATE "/ENTRY:mainCRTStartup")
    endif ()
endfunction()

# Gives a bundled app its at-rest icon — the one Finder, the Applications
# folder and a not-yet-running Dock tile draw. Those are rendered by other
# processes that never execute the binary, so a runtime
# setApplicationIconImage cannot supply them: macOS reads
# Contents/Resources/AppIcon.icns, named by CFBundleIconFile in Info.plist.
# This generates that .icns at build time from a single PNG master
# (1024x1024 with transparency recommended) via sips + iconutil.
#
# macOS-only for now. The Windows analogue is an ICON resource compiled into
# the .exe from an .rc file — not yet wired; Windows apps keep the runtime
# WindowOptions::applicationIcon path.
function(eacp_set_app_icon target icon)
    if (NOT APPLE OR IOS)
        return()
    endif ()

    get_filename_component(icon "${icon}" ABSOLUTE)
    set(iconset_dir "${CMAKE_CURRENT_BINARY_DIR}/${target}.iconset")
    set(icns_file "${CMAKE_CURRENT_BINARY_DIR}/${target}-icon/AppIcon.icns")

    # iconutil requires the exact icon_<N>x<N>[@2x].png names inside the
    # .iconset; sips scales the master to each slot.
    set(resize_commands)
    foreach (size IN ITEMS 16 32 128 256 512)
        math(EXPR retina "${size} * 2")
        list(APPEND resize_commands
                COMMAND sips -z ${size} ${size} "${icon}"
                --out "${iconset_dir}/icon_${size}x${size}.png"
                COMMAND sips -z ${retina} ${retina} "${icon}"
                --out "${iconset_dir}/icon_${size}x${size}@2x.png")
    endforeach ()

    add_custom_command(OUTPUT "${icns_file}"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${iconset_dir}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${iconset_dir}"
            ${resize_commands}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/${target}-icon"
            COMMAND iconutil -c icns -o "${icns_file}" "${iconset_dir}"
            DEPENDS "${icon}"
            COMMENT "Generating ${target} AppIcon.icns"
            VERBATIM)

    target_sources(${target} PRIVATE "${icns_file}")
    set_source_files_properties("${icns_file}" PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources
            GENERATED TRUE)
    set_target_properties(${target} PROPERTIES
            MACOSX_BUNDLE_ICON_FILE AppIcon.icns)
endfunction()

function(add_ide_sources target)
    file(GLOB_RECURSE ALL_HEADERS
            "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    )

    if (ALL_HEADERS)
        target_sources(${target} PRIVATE ${ALL_HEADERS})
        set_source_files_properties(${ALL_HEADERS} PROPERTIES HEADER_FILE_ONLY TRUE)
    endif ()
endfunction()

function(eacp_default_setup)
    eacp_setup_apple()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_COMPILE_OPTIONS_IPO "-flto=full")
    endif ()
endfunction()