# A runtime-loaded eacp plugin: a MODULE library that statically links its
# own eacp copy. eacp and its dependencies compile with hidden visibility, so
# the module re-exports none of their symbols; plugin entry points are marked
# with EACP_PLUGIN_EXPORT (Core/Plugins/PluginExport.h), and the visibility
# of the plugin's own remaining code is the plugin author's choice. A build
# with eacp_add_hot_reload ON compiles eacp at default visibility instead, so
# plugins built in it re-export eacp and lose that isolation.
#
#   eacp_add_plugin(MyPlugin Plugin.cpp)
#   target_link_libraries(MyPlugin PRIVATE eacp-core)
function(eacp_add_plugin target)
    add_library(${target} MODULE ${ARGN})

    set_target_properties(${target} PROPERTIES
            PREFIX ""
            # CMake defines <target>_EXPORTS for a MODULE by default. eacp marks
            # its plugin entry points with EACP_PLUGIN_EXPORT and nothing reads
            # the generated macro, so drop it rather than leave every plugin
            # carrying a define the shared PCH image cannot match.
            DEFINE_SYMBOL ""
            POSITION_INDEPENDENT_CODE ON)

    # Keep the plugin's own sources free of STB_GNU_UNIQUE symbols so glibc can
    # unmap it on the last close -- see the eacp-wide flag in the root
    # CMakeLists that covers the eacp libraries linked in here.
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target} PRIVATE -fno-gnu-unique)
    endif ()

    set_default_target_setting(${target})
    eacp_enable_unity_build(${target})
endfunction()

function(eacp_collect_link_targets target out_var)
    set(found ${${out_var}})
    get_target_property(links ${target} LINK_LIBRARIES)
    get_target_property(interface_links ${target} INTERFACE_LINK_LIBRARIES)

    foreach (link IN LISTS links interface_links)
        string(REGEX REPLACE "\\$<LINK_ONLY:([^>]*)>" "\\1" link "${link}")

        if (NOT TARGET "${link}")
            continue()
        endif ()

        get_target_property(aliased ${link} ALIASED_TARGET)

        if (aliased)
            set(link ${aliased})
        endif ()

        get_target_property(imported ${link} IMPORTED)

        if (imported OR link IN_LIST found)
            continue()
        endif ()

        list(APPEND found ${link})
        eacp_collect_link_targets(${link} found)
    endforeach ()

    set(${out_var} ${found} PARENT_SCOPE)
endfunction()

# The targets whose usage requirements reach a compile of `target`: its links,
# then each one's interface links, but not $<LINK_ONLY:...> (a static
# library's private dependencies, needed only to link).
function(eacp_collect_usage_targets target property out_var)
    set(found ${${out_var}})
    get_target_property(links ${target} ${property})

    foreach (link IN LISTS links)
        if (NOT TARGET "${link}")
            continue()
        endif ()

        get_target_property(aliased ${link} ALIASED_TARGET)

        if (aliased)
            set(link ${aliased})
        endif ()

        if (link IN_LIST found)
            continue()
        endif ()

        list(APPEND found ${link})
        eacp_collect_usage_targets(${link} INTERFACE_LINK_LIBRARIES found)
    endforeach ()

    set(${out_var} ${found} PARENT_SCOPE)
endfunction()

# The include directories, definitions, options and features the module needs
# to compile like the app, without $<COMPILE_ONLY:...>, which also brings in
# INTERFACE_SOURCES: generated resource registrations whose static
# initialisers would rerun in every loaded generation.
function(eacp_copy_usage_requirements module)
    foreach (target IN LISTS ARGN)
        foreach (property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS
                COMPILE_OPTIONS COMPILE_FEATURES)
            get_target_property(values ${target} INTERFACE_${property})

            if (values)
                set_property(TARGET ${module} APPEND PROPERTY
                        ${property} ${values})
            endif ()
        endforeach ()

        get_target_property(system_includes ${target}
                INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)

        if (system_includes)
            target_include_directories(${module} SYSTEM PRIVATE
                    ${system_includes})
        endif ()
    endforeach ()
endfunction()

function(eacp_set_default_visibility target)
    set_target_properties(${target} PROPERTIES
            C_VISIBILITY_PRESET default
            CXX_VISIBILITY_PRESET default
            OBJC_VISIBILITY_PRESET default
            OBJCXX_VISIBILITY_PRESET default
            VISIBILITY_INLINES_HIDDEN OFF)
endfunction()

function(eacp_link_hot_reload app module library)
    get_target_property(direct ${app} LINK_LIBRARIES)
    set(closure "")
    eacp_collect_link_targets(${app} closure)

    if (library AND NOT library IN_LIST closure)
        message(FATAL_ERROR "eacp_add_hot_reload(${app}): LIBRARY ${library} "
                "is not linked into ${app}")
    endif ()
    set(archives "")

    foreach (target IN LISTS closure)
        get_target_property(type ${target} TYPE)

        if (type STREQUAL "STATIC_LIBRARY")
            list(APPEND archives ${target})
        endif ()

        if (type MATCHES "^(STATIC|OBJECT)_LIBRARY$")
            eacp_set_default_visibility(${target})
        endif ()
    endforeach ()

    set(usage_targets "")
    eacp_collect_usage_targets(${app} LINK_LIBRARIES usage_targets)
    eacp_copy_usage_requirements(${module} ${usage_targets})

    set(links "")

    foreach (link IN LISTS direct)
        if (TARGET "${link}")
            add_dependencies(${module} ${link})
        endif ()

        if (NOT link IN_LIST archives)
            list(APPEND links "${link}")
        endif ()
    endforeach ()

    foreach (archive IN LISTS archives)
        set(whole "$<LINK_LIBRARY:WHOLE_ARCHIVE,${archive}>")

        if (archive IN_LIST direct)
            list(APPEND links "${whole}")
        else ()
            list(APPEND links "$<LINK_ONLY:${whole}>")
        endif ()

        set_target_properties(${app} PROPERTIES
                LINK_LIBRARY_OVERRIDE_${archive} WHOLE_ARCHIVE)
    endforeach ()

    set_target_properties(${app} PROPERTIES
            LINK_LIBRARIES "${links}"
            INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
endfunction()

function(eacp_collect_build_targets directory out_var)
    set(found ${${out_var}})
    get_property(targets DIRECTORY ${directory} PROPERTY BUILDSYSTEM_TARGETS)
    list(APPEND found ${targets})
    get_property(subdirectories DIRECTORY ${directory} PROPERTY SUBDIRECTORIES)

    foreach (subdirectory IN LISTS subdirectories)
        eacp_collect_build_targets(${subdirectory} found)
    endforeach ()

    set(${out_var} ${found} PARENT_SCOPE)
endfunction()

# EACP_RELOADABLE adds a vtable pointer, so every translation unit in the build
# must agree on EACP_HOT_RELOAD. A directory-scope definition only reaches
# targets made after it, so this runs once at the end of the configure and
# defines it on every compiled target.
function(eacp_define_hot_reload_everywhere)
    set(targets "")
    eacp_collect_build_targets(${CMAKE_SOURCE_DIR} targets)

    foreach (target IN LISTS targets)
        get_target_property(type ${target} TYPE)

        if (type MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY|EXECUTABLE)$")
            target_compile_definitions(${target} PRIVATE EACP_HOT_RELOAD=1)
        endif ()
    endforeach ()
endfunction()

# Deferred to the end of the configure, once the app's links are final.
#
#   eacp_add_hot_reload(MyApp MODULE MyAppLayout [LIBRARY MyLib]
#           SOURCES Layout.cpp [HOT_RELOAD ON|OFF])
#
# Without LIBRARY the sources are the app's and are compiled into it. With
# LIBRARY they are that static library's, relative to its directory: the app
# gets them by linking it, and the module compiles them with its flags.
function(eacp_add_hot_reload app)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "MODULE;HOT_RELOAD;LIBRARY"
            "SOURCES")

    set(enabled ${EACP_HOT_RELOAD})

    if (DEFINED ARG_HOT_RELOAD)
        if (NOT ARG_HOT_RELOAD MATCHES "^(ON|OFF)$")
            message(FATAL_ERROR "eacp_add_hot_reload(${app}): HOT_RELOAD is ON "
                    "or OFF, not '${ARG_HOT_RELOAD}'")
        endif ()

        set(enabled ${ARG_HOT_RELOAD})
    endif ()

    if (IOS OR ANDROID OR WIN32)
        set(enabled OFF)
    endif ()

    set(sources ${ARG_SOURCES})
    set(source_dir ${CMAKE_CURRENT_SOURCE_DIR})

    if (DEFINED ARG_LIBRARY)
        if (NOT TARGET ${ARG_LIBRARY})
            message(FATAL_ERROR "eacp_add_hot_reload(${app}): LIBRARY "
                    "${ARG_LIBRARY} is not a target")
        endif ()

        get_target_property(aliased_library ${ARG_LIBRARY} ALIASED_TARGET)

        if (aliased_library)
            set(ARG_LIBRARY ${aliased_library})
        endif ()

        get_target_property(library_type ${ARG_LIBRARY} TYPE)

        if (NOT library_type MATCHES "^(STATIC|OBJECT)_LIBRARY$")
            message(FATAL_ERROR "eacp_add_hot_reload(${app}): LIBRARY "
                    "${ARG_LIBRARY} is a ${library_type}, not a static or "
                    "object library")
        endif ()

        get_target_property(source_dir ${ARG_LIBRARY} SOURCE_DIR)
        set(absolute_sources "")

        foreach (source IN LISTS sources)
            cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY "${source_dir}"
                    NORMALIZE OUTPUT_VARIABLE absolute_source)
            list(APPEND absolute_sources "${absolute_source}")
        endforeach ()

        set(sources ${absolute_sources})
    else ()
        target_sources(${app} PRIVATE ${sources})
    endif ()

    if (NOT enabled)
        message(STATUS "${app}: hot reload OFF")
        return()
    endif ()

    message(STATUS "${app}: hot reload ON")

    get_property(defined_everywhere GLOBAL PROPERTY EACP_HOT_RELOAD_EVERYWHERE)

    if (NOT defined_everywhere)
        set_property(GLOBAL PROPERTY EACP_HOT_RELOAD_EVERYWHERE ON)
        cmake_language(EVAL CODE "
            cmake_language(DEFER DIRECTORY [[${CMAKE_SOURCE_DIR}]]
                    CALL eacp_define_hot_reload_everywhere)")
    endif ()

    set_target_properties(${app} PROPERTIES ENABLE_EXPORTS ON)
    eacp_set_default_visibility(${app})

    eacp_add_plugin(${ARG_MODULE} ${sources})
    add_dependencies(${app} ${ARG_MODULE})
    eacp_set_default_visibility(${ARG_MODULE})
    set_target_properties(${ARG_MODULE} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)

    if (APPLE)
        target_link_options(${app} PRIVATE LINKER:-export_dynamic)
        target_link_options(${ARG_MODULE} PRIVATE
                LINKER:-undefined,dynamic_lookup)
    endif ()

    if (DEFINED ARG_LIBRARY)
        add_dependencies(${ARG_MODULE} ${ARG_LIBRARY})

        foreach (property IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS
                COMPILE_OPTIONS)
            get_target_property(values ${ARG_LIBRARY} ${property})

            if (values)
                set_property(TARGET ${ARG_MODULE} APPEND PROPERTY
                        ${property} ${values})
            endif ()
        endforeach ()
    endif ()

    cmake_language(EVAL CODE "
        cmake_language(DEFER DIRECTORY [[${CMAKE_SOURCE_DIR}]]
                CALL eacp_link_hot_reload [[${app}]] [[${ARG_MODULE}]]
                [[${ARG_LIBRARY}]])")

    target_compile_definitions(${app} PRIVATE
            EACP_HOT_RELOAD_LIBRARY="$<TARGET_FILE:${ARG_MODULE}>"
            EACP_HOT_RELOAD_CMAKE="${CMAKE_COMMAND}"
            EACP_HOT_RELOAD_BUILD_DIR="${CMAKE_BINARY_DIR}"
            EACP_HOT_RELOAD_CONFIG="$<CONFIG>"
            EACP_HOT_RELOAD_TARGET="${ARG_MODULE}"
            EACP_HOT_RELOAD_SOURCE_DIR="${source_dir}")
endfunction()
