set(CMAKE_FOLDER Examples/ReactLike)

add_executable(ReactLike Main.cpp)
target_link_libraries(ReactLike PRIVATE eacp-react)

set(BUNDLE_ID "com.eacp.reactlike")

set_target_properties(ReactLike PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "ReactLike"
        MACOSX_BUNDLE_GUI_IDENTIFIER ${BUNDLE_ID}
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${BUNDLE_ID})

eacp_set_gui_subsystem(ReactLike)
set_default_target_setting(ReactLike)
