add_executable(ServingHeavyContent Main.cpp)
target_link_libraries(ServingHeavyContent PRIVATE eacp-webview)

# The heavy demo clip is NOT committed (it'd bloat the repo). Fetch it at
# configure time into the embedded dist — Big Buck Bunny, a ~10 MB 1080p H.264
# clip (CC-BY, (c) Blender Foundation). EXPECTED_HASH makes this idempotent:
# CMake skips the download when the file already exists and matches.
set(HEAVY_MP4 "${CMAKE_CURRENT_SOURCE_DIR}/web/dist/heavy.mp4")
file(DOWNLOAD
        "https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/1080/Big_Buck_Bunny_1080_10s_10MB.mp4"
        "${HEAVY_MP4}"
        EXPECTED_HASH SHA256=1bb2a62abfcb5c4bf2e9216b63ba715b358f22c9fa8e68ca7149c4788b906826
        TLS_VERIFY ON
        INACTIVITY_TIMEOUT 60
        STATUS HEAVY_DL_STATUS)
list(GET HEAVY_DL_STATUS 0 HEAVY_DL_CODE)
if(NOT HEAVY_DL_CODE EQUAL 0)
    list(GET HEAVY_DL_STATUS 1 HEAVY_DL_MSG)
    file(REMOVE "${HEAVY_MP4}")
    message(FATAL_ERROR "ServingHeavyContent: could not fetch the demo clip "
            "(${HEAVY_DL_MSG}). It's a ~10 MB download; check network access.")
endif()

# No package.json under web/ -> eacp_webview_add_vite embeds the prebuilt dist/
# as-is (index.html + the fetched heavy.mp4). The embedded scheme serves them;
# see registerEmbeddedScheme in WebView-Shared.cpp — it now streams with
# byte-range support, which is what makes the embedded <video> playable.
eacp_webview_add_vite(ServingHeavyContent
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/web
        NAMESPACE WebResources
        CATEGORY ServingHeavyContent)

set(BUNDLE_ID "com.eacp.servingheavycontent")

set_target_properties(ServingHeavyContent PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Serving Heavy Content"
        MACOSX_BUNDLE_GUI_IDENTIFIER ${BUNDLE_ID}
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${BUNDLE_ID})

eacp_set_gui_subsystem(ServingHeavyContent)
set_default_target_setting(ServingHeavyContent)
