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

# Stamps the version identity a platform shows without running the binary,
# taken from the version CMake already knows — the enclosing project(...
# VERSION x.y.z): a VERSIONINFO resource on Windows (Explorer's Details tab,
# installers, crash tooling), the bundle Info.plist version keys on macOS
# (Get Info, crash reports). Runs from eacp_set_gui_subsystem so every eacp
# GUI app is stamped with no extra calls; a project without a VERSION is
# left unstamped. An app that versions independently of the tree declares
# its own project() — PROJECT_VERSION is the nearest enclosing one.
#
# EACP_COMPANY_NAME (a variable set by the consuming tree) populates
# CompanyName + LegalCopyright; both are omitted when unset. The product
# name is the target's MACOSX_BUNDLE_BUNDLE_NAME when set (as
# eacp_add_webview_app does), else the target name.
function(_eacp_stamp_version_info target)
    set(version "${PROJECT_VERSION}")
    if (NOT version)
        return()
    endif ()

    get_target_property(product_name ${target} MACOSX_BUNDLE_BUNDLE_NAME)
    if (NOT product_name)
        set(product_name "${target}")
    endif ()

    # Numeric version fields want exactly four integer parts; pad project()'s
    # 1-4 dotted components with zeros ("1.2" -> 1.2.0.0).
    string(REPLACE "." ";" version_parts "${version}")
    list(LENGTH version_parts part_count)
    while (part_count LESS 4)
        list(APPEND version_parts 0)
        list(LENGTH version_parts part_count)
    endwhile ()
    list(SUBLIST version_parts 0 4 version_parts)
    list(SUBLIST version_parts 0 3 short_version_parts)
    list(JOIN version_parts "," rc_version)
    list(JOIN short_version_parts "." short_version)

    if (APPLE)
        # Consumed by the Info.plist template when the target is a bundle
        # (CFBundleShortVersionString / CFBundleVersion /
        # NSHumanReadableCopyright). Harmless on non-bundle targets.
        set_target_properties(${target} PROPERTIES
                MACOSX_BUNDLE_SHORT_VERSION_STRING "${short_version}"
                MACOSX_BUNDLE_BUNDLE_VERSION "${version}"
                MACOSX_BUNDLE_LONG_VERSION_STRING "${version}")
        if (EACP_COMPANY_NAME)
            set_target_properties(${target} PROPERTIES
                    MACOSX_BUNDLE_COPYRIGHT
                    "Copyright (C) ${EACP_COMPANY_NAME}")
        endif ()
        return()
    endif ()

    if (NOT WIN32)
        return()
    endif ()

    # rc string literals escape embedded quotes by doubling them.
    string(REPLACE "\"" "\"\"" product_name "${product_name}")

    set(string_values "")
    if (EACP_COMPANY_NAME)
        string(REPLACE "\"" "\"\"" company "${EACP_COMPANY_NAME}")
        string(APPEND string_values
                "            VALUE \"CompanyName\", \"${company}\"\n"
                "            VALUE \"LegalCopyright\", \"Copyright (C) ${company}\"\n")
    endif ()
    string(APPEND string_values
            "            VALUE \"FileDescription\", \"${product_name}\"\n"
            "            VALUE \"FileVersion\", \"${version}\"\n"
            "            VALUE \"InternalName\", \"${target}\"\n"
            "            VALUE \"OriginalFilename\", \"${target}.exe\"\n"
            "            VALUE \"ProductName\", \"${product_name}\"\n"
            "            VALUE \"ProductVersion\", \"${version}\"")

    # Raw resource id and numeric flags so the .rc compiles without winres.h:
    # 1 = VS_VERSION_INFO, 0x3f = VS_FFI_FILEFLAGSMASK, 0x40004 =
    # VOS_NT_WINDOWS32, 0x1 = VFT_APP; "040904B0" / 0x409,1200 = en-US, Unicode.
    set(rc_file "${CMAKE_CURRENT_BINARY_DIR}/${target}-version.rc")
    file(CONFIGURE OUTPUT "${rc_file}" @ONLY CONTENT [[1 VERSIONINFO
FILEVERSION @rc_version@
PRODUCTVERSION @rc_version@
FILEFLAGSMASK 0x3fL
FILEFLAGS 0x0L
FILEOS 0x40004L
FILETYPE 0x1L
FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904B0"
        BEGIN
@string_values@
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END
]])
    target_sources(${target} PRIVATE "${rc_file}")
endfunction()

# Mark an executable as a windowed GUI app so launching it never pops a console
# window on Windows — the cross-platform analogue of MACOSX_BUNDLE on Apple. A
# no-op everywhere but Windows. Call this on any eacp app that owns a window
# (graphics/tray/webview apps) rather than a console. Also stamps the app with
# the enclosing project()'s VERSION (see _eacp_stamp_version_info above).
function(eacp_set_gui_subsystem target)
    set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE TRUE)
    # WIN32_EXECUTABLE selects /SUBSYSTEM:WINDOWS, whose default MSVC entry point
    # is WinMainCRTStartup (it expects WinMain). eacp apps use a standard
    # int main(), so redirect the entry to the console CRT startup — it still
    # parses argv and calls main(), just without a console attached.
    if (MSVC)
        target_link_options(${target} PRIVATE "/ENTRY:mainCRTStartup")
    endif ()

    _eacp_stamp_version_info(${target})
endfunction()

# Gives an app its at-rest icon — the one Finder, Explorer and a
# not-yet-running Dock/taskbar tile draw. Those are rendered by processes
# that never execute the binary, so a runtime WindowOptions::applicationIcon
# cannot supply them: macOS reads the bundle's .icns (named by
# CFBundleIconFile), Windows reads an ICON resource compiled into the .exe.
# Apps either point IMAGE at one source PNG/JPEG and let the build generate the
# right container for the platform being built, or supply the platform-native
# files directly:
#
#   eacp_set_app_icon(MyApp IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/Icon.png")
#
#   eacp_set_app_icon(MyApp
#           ICNS "${CMAKE_CURRENT_SOURCE_DIR}/Icon.icns"
#           ICO  "${CMAKE_CURRENT_SOURCE_DIR}/Icon.ico")
#
# IMAGE runs eacp-icon-tool at build time to emit .icns on macOS / .ico on
# Windows, and wins over a prebuilt ICNS/ICO for that platform. Any argument
# may be omitted for a platform the app doesn't ship on. Explorer picks the
# ICON resource with the lowest ID for the at-rest icon, hence resource id 1 in
# the generated .rc.
function(eacp_set_app_icon target)
    cmake_parse_arguments(ARG "" "ICNS;ICO;IMAGE" "" ${ARGN})

    # A source image generates the current platform's container into the build
    # tree; feed it into the same wiring the prebuilt paths use below.
    if (ARG_IMAGE)
        get_filename_component(image "${ARG_IMAGE}" ABSOLUTE)

        if (APPLE AND NOT IOS)
            set(generated_icns "${CMAKE_CURRENT_BINARY_DIR}/${target}.icns")
            add_custom_command(
                    OUTPUT "${generated_icns}"
                    COMMAND eacp-icon-tool --format icns
                            --in "${image}" --out "${generated_icns}"
                    DEPENDS eacp-icon-tool "${image}"
                    VERBATIM)
            set(ARG_ICNS "${generated_icns}")
        elseif (WIN32)
            set(generated_ico "${CMAKE_CURRENT_BINARY_DIR}/${target}.ico")
            add_custom_command(
                    OUTPUT "${generated_ico}"
                    COMMAND eacp-icon-tool --format ico
                            --in "${image}" --out "${generated_ico}"
                    DEPENDS eacp-icon-tool "${image}"
                    VERBATIM)
            # The .rc references the .ico via OBJECT_DEPENDS rather than
            # compiling it, so list the generated file as a non-compiled source
            # to make CMake schedule its custom command.
            set_source_files_properties("${generated_ico}" PROPERTIES
                    GENERATED TRUE HEADER_FILE_ONLY TRUE)
            target_sources(${target} PRIVATE "${generated_ico}")
            set(ARG_ICO "${generated_ico}")
        endif ()
    endif ()

    if (APPLE AND NOT IOS AND ARG_ICNS)
        get_filename_component(icns "${ARG_ICNS}" ABSOLUTE)
        get_filename_component(icns_name "${icns}" NAME)

        target_sources(${target} PRIVATE "${icns}")
        set_source_files_properties("${icns}" PROPERTIES
                MACOSX_PACKAGE_LOCATION Resources)
        set_target_properties(${target} PROPERTIES
                MACOSX_BUNDLE_ICON_FILE "${icns_name}")
    endif ()

    if (WIN32 AND ARG_ICO)
        # get_filename_component normalises to forward slashes, which rc.exe
        # accepts and which don't act as escape characters in the .rc string.
        get_filename_component(ico "${ARG_ICO}" ABSOLUTE)
        set(rc_file "${CMAKE_CURRENT_BINARY_DIR}/${target}-icon.rc")

        file(CONFIGURE OUTPUT "${rc_file}" CONTENT "1 ICON \"${ico}\"\n")
        target_sources(${target} PRIVATE "${rc_file}")
        set_source_files_properties("${rc_file}" PROPERTIES
                OBJECT_DEPENDS "${ico}")
    endif ()
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
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
    add_compile_definitions(_LIBCPP_REMOVE_TRANSITIVE_INCLUDES)
    eacp_setup_apple()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_COMPILE_OPTIONS_IPO "-flto=full")
    endif ()
endfunction()