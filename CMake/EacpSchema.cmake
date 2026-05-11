# eacp_target_uses_schema — wire a target to consume a Miro schema.
#
# Links the schema library (so the consumer inherits the generated
# header include directories) and adds a build-order dependency on
# the schema's codegen target (so the generated headers exist before
# the consumer's compile step starts). The HANDLERS keyword switches
# the link to WHOLE_ARCHIVE so the MIRO_EXPORT_COMMAND static
# initializers survive into the consumer's binary — needed for any
# target that dispatches commands via Miro::Bridge::useStaticRegistry().
# Plain (no HANDLERS) is the right choice for clients that only
# consume the generated typed wrapper headers.
#
# The codegen dep is added here rather than on the schema library
# itself because the codegen executable links the library
# WHOLE_ARCHIVE — making the library depend on codegen would form a
# CMake dependency cycle that's not allowed across library/executable
# boundaries.
#
# Usage:
#   eacp_target_uses_schema(MyServer MySchema HANDLERS)
#   eacp_target_uses_schema(MyClient MySchema)
function(eacp_target_uses_schema target schema)
    cmake_parse_arguments(EUS "HANDLERS" "" "" ${ARGN})

    if (NOT TARGET ${target})
        message(FATAL_ERROR
            "eacp_target_uses_schema: target '${target}' does not exist")
    endif ()
    if (NOT TARGET ${schema})
        message(FATAL_ERROR
            "eacp_target_uses_schema: schema '${schema}' does not exist; "
            "call miro_export(${schema} ...) first")
    endif ()

    if (EUS_HANDLERS)
        target_link_libraries(${target} PRIVATE
            "$<LINK_LIBRARY:WHOLE_ARCHIVE,${schema}>")
    else ()
        target_link_libraries(${target} PRIVATE ${schema})
    endif ()

    if (TARGET ${schema}_codegen)
        add_dependencies(${target} ${schema}_codegen)
    endif ()
endfunction()
