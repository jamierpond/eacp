add_executable(ServingHeavyContent Main.cpp)
target_link_libraries(ServingHeavyContent PRIVATE eacp-webview)

# The demo clips are NOT committed (they'd bloat the repo). Fetch them at
# configure time into the embedded dist — Big Buck Bunny / Jellyfish / Sintel,
# ~25 MB of 1080p + 720p H.264 (CC-BY; (c) Blender Foundation, and the Jellyfish
# sample from test-videos.co.uk). EXPECTED_HASH makes this idempotent: CMake
# skips a download when the file already exists and matches.
#
# Four clips rather than one, because the interaction being demonstrated is
# hover switching between heavy clips — a single clip can't show that hovering
# never re-fetches.
set(HEAVY_CLIPS
        "heavy.mp4|https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/1080/Big_Buck_Bunny_1080_10s_10MB.mp4|1bb2a62abfcb5c4bf2e9216b63ba715b358f22c9fa8e68ca7149c4788b906826"
        "jellyfish.mp4|https://test-videos.co.uk/vids/jellyfish/mp4/h264/720/Jellyfish_720_10s_5MB.mp4|533d39108871a6d1ff19bc49c6f292ba9046d64f24157be1dad321d39525c6de"
        "sintel.mp4|https://test-videos.co.uk/vids/sintel/mp4/h264/720/Sintel_720_10s_5MB.mp4|52f127a6e77a8544c80ef635f82fc1098615af1a8fc38b2c5c1bd10a740a7ee8"
        "bunny720.mp4|https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/720/Big_Buck_Bunny_720_10s_5MB.mp4|bfb0b4b07b8bb61b707a052e62daec31305a13276fcbebc714f2462f31d96210")

foreach(HEAVY_CLIP IN LISTS HEAVY_CLIPS)
    string(REPLACE "|" ";" HEAVY_CLIP_PARTS "${HEAVY_CLIP}")
    list(GET HEAVY_CLIP_PARTS 0 HEAVY_CLIP_NAME)
    list(GET HEAVY_CLIP_PARTS 1 HEAVY_CLIP_URL)
    list(GET HEAVY_CLIP_PARTS 2 HEAVY_CLIP_HASH)
    set(HEAVY_MP4 "${CMAKE_CURRENT_SOURCE_DIR}/web/dist/${HEAVY_CLIP_NAME}")
    file(DOWNLOAD
            "${HEAVY_CLIP_URL}"
            "${HEAVY_MP4}"
            EXPECTED_HASH SHA256=${HEAVY_CLIP_HASH}
            TLS_VERIFY ON
            INACTIVITY_TIMEOUT 60
            STATUS HEAVY_DL_STATUS)
    list(GET HEAVY_DL_STATUS 0 HEAVY_DL_CODE)
    if(NOT HEAVY_DL_CODE EQUAL 0)
        list(GET HEAVY_DL_STATUS 1 HEAVY_DL_MSG)
        file(REMOVE "${HEAVY_MP4}")
        message(FATAL_ERROR "ServingHeavyContent: could not fetch ${HEAVY_CLIP_NAME} "
                "(${HEAVY_DL_MSG}). The clips are a ~25 MB download; check network access.")
    endif()
endforeach()

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
