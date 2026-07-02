# Application icons from plain PNGs. eacp_set_icon() packs the given PNGs
# into the native container at build time with the EacpIconPacker host tool:
# on Windows an .ico embedded through a generated resource script, on macOS
# an .icns copied into the bundle's Resources. Calling it without MAIN_ICON
# applies the eacp default icon, and eacp_set_gui_subsystem() schedules that
# same default for every GUI app, so no windowed app ships with the generic
# system icon.
#
# The Windows resource IDs are shared with Window-Windows.cpp
# (WindowOptions::useEmbeddedApplicationIcon): 1 is the application icon,
# 2 the optional Alt-Tab override from WINDOWS_ALT_TAB_ICON.

set(EACP_DEFAULT_ICON "${CMAKE_CURRENT_LIST_DIR}/Icons/eacp-default.png"
        CACHE INTERNAL "PNG applied when eacp_set_icon gets no MAIN_ICON")

get_filename_component(EACP_ICON_PACKER_SOURCE
        "${CMAKE_CURRENT_LIST_DIR}/../Tools/IconPacker/Main.cpp" ABSOLUTE)
set(EACP_ICON_PACKER_SOURCE "${EACP_ICON_PACKER_SOURCE}"
        CACHE INTERNAL "Source of the icon packing host tool")

function(eacp_set_icon target)
    cmake_parse_arguments(ARG "" "" "MAIN_ICON;WINDOWS_ALT_TAB_ICON" ${ARGN})

    _eacp_resolve_icon_paths(mainIcons "${ARG_MAIN_ICON}")
    _eacp_resolve_icon_paths(altTabIcons "${ARG_WINDOWS_ALT_TAB_ICON}")

    set_target_properties(${target} PROPERTIES
            EACP_MAIN_ICON "${mainIcons}"
            EACP_WINDOWS_ALT_TAB_ICON "${altTabIcons}")

    _eacp_schedule_icon_setup(${target})
endfunction()

function(_eacp_resolve_icon_paths outVar paths)
    set(resolved "")
    foreach (path IN LISTS paths)
        if (NOT IS_ABSOLUTE "${path}")
            set(path "${CMAKE_CURRENT_SOURCE_DIR}/${path}")
        endif ()
        list(APPEND resolved "${path}")
    endforeach ()
    set(${outVar} "${resolved}" PARENT_SCOPE)
endfunction()

# Attaching is deferred to the end of the directory so it runs after the
# app's CMakeLists is fully processed — the icon lands correctly no matter
# where MACOSX_BUNDLE, eacp_set_gui_subsystem() or eacp_set_icon() appear.
function(_eacp_schedule_icon_setup target)
    get_target_property(scheduled ${target} EACP_ICON_SETUP_SCHEDULED)
    if (scheduled)
        return()
    endif ()

    set_target_properties(${target} PROPERTIES EACP_ICON_SETUP_SCHEDULED TRUE)

    # DEFER CALL arguments are expanded when the deferred call runs, where
    # ${target} no longer exists — EVAL forces expansion now.
    cmake_language(EVAL CODE
            "cmake_language(DEFER CALL _eacp_attach_icon [[${target}]])")
endfunction()

function(_eacp_attach_icon target)
    get_target_property(mainIcons ${target} EACP_MAIN_ICON)
    if (NOT mainIcons)
        set(mainIcons "${EACP_DEFAULT_ICON}")
    endif ()

    get_target_property(altTabIcons ${target} EACP_WINDOWS_ALT_TAB_ICON)
    if (NOT altTabIcons)
        set(altTabIcons "")
    endif ()

    if (WIN32)
        _eacp_attach_windows_icon(${target} "${mainIcons}" "${altTabIcons}")
    elseif (APPLE AND NOT IOS)
        _eacp_attach_macos_icon(${target} "${mainIcons}")
    endif ()
endfunction()

function(_eacp_ensure_icon_packer)
    if (TARGET EacpIconPacker)
        return()
    endif ()

    add_executable(EacpIconPacker "${EACP_ICON_PACKER_SOURCE}")
    set_target_properties(EacpIconPacker PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            FOLDER Tools)
endfunction()

function(_eacp_pack_icons format output pngs)
    add_custom_command(OUTPUT "${output}"
            COMMAND EacpIconPacker ${format} "${output}" ${pngs}
            DEPENDS ${pngs} EacpIconPacker
            COMMENT "Packing ${output}"
            VERBATIM)
endfunction()

function(_eacp_attach_windows_icon target mainIcons altTabIcons)
    _eacp_ensure_icon_packer()

    set(iconDir "${CMAKE_CURRENT_BINARY_DIR}/${target}-icons")
    set(mainIco "${iconDir}/main.ico")
    _eacp_pack_icons(ico "${mainIco}" "${mainIcons}")

    set(icoFiles "${mainIco}")
    set(rcContent "1 ICON \"main.ico\"\n")

    if (altTabIcons)
        set(altTabIco "${iconDir}/alttab.ico")
        _eacp_pack_icons(ico "${altTabIco}" "${altTabIcons}")
        list(APPEND icoFiles "${altTabIco}")
        string(APPEND rcContent "2 ICON \"alttab.ico\"\n")
    endif ()

    set(rcFile "${iconDir}/${target}-icon.rc")
    file(CONFIGURE OUTPUT "${rcFile}" CONTENT "${rcContent}")

    set_source_files_properties("${rcFile}" PROPERTIES
            OBJECT_DEPENDS "${icoFiles}")
    target_sources(${target} PRIVATE "${rcFile}")
endfunction()

function(_eacp_attach_macos_icon target mainIcons)
    get_target_property(isBundle ${target} MACOSX_BUNDLE)
    if (NOT isBundle)
        return()
    endif ()

    _eacp_ensure_icon_packer()

    set(icnsFile "${CMAKE_CURRENT_BINARY_DIR}/${target}-icons/${target}.icns")
    _eacp_pack_icons(icns "${icnsFile}" "${mainIcons}")

    set_source_files_properties("${icnsFile}" PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources
            GENERATED TRUE)
    target_sources(${target} PRIVATE "${icnsFile}")
    set_target_properties(${target} PROPERTIES
            MACOSX_BUNDLE_ICON_FILE "${target}.icns")
endfunction()
