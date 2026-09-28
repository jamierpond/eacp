include(CPM)

# FreeType rasterizes the /system/fonts faces behind Android's 2D tier. Every
# optional codec is off, so the NDK build needs nothing from the host. In a
# function so nothing leaks past the fetch; CMP0077 is NEW, so these beat its
# option()s.
function(eacp_add_freetype)
    set(BUILD_SHARED_LIBS OFF)
    set(SKIP_INSTALL_ALL ON)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    set(FT_DISABLE_ZLIB ON)
    set(FT_DISABLE_BZIP2 ON)
    set(FT_DISABLE_PNG ON)
    set(FT_DISABLE_HARFBUZZ ON)
    set(FT_DISABLE_BROTLI ON)
    set(FT_REQUIRE_ZLIB OFF)
    set(FT_REQUIRE_BZIP2 OFF)
    set(FT_REQUIRE_PNG OFF)
    set(FT_REQUIRE_HARFBUZZ OFF)
    set(FT_REQUIRE_BROTLI OFF)

    CPMAddPackage(
            NAME freetype
            GITHUB_REPOSITORY freetype/freetype
            GIT_TAG VER-2-13-3
            GIT_SHALLOW YES
            SYSTEM YES
            ${EACP_FETCH_QUIET})

    set_target_properties(freetype PROPERTIES FOLDER "${CMAKE_FOLDER}")
    silence_target_warnings(freetype)
    eacp_force_optimization(freetype)
endfunction()

if (NOT TARGET eacp-freetype)
    eacp_add_freetype()

    add_library(eacp-freetype INTERFACE)
    target_link_libraries(eacp-freetype INTERFACE freetype)
endif ()
