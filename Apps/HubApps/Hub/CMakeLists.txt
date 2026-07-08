add_executable(Hub Main.cpp)
target_link_libraries(Hub PRIVATE eacp-graphics HubRpc)

set(BUNDLE_ID "com.eacp.hubapp")

set_target_properties(Hub PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "EACP Hub"
        MACOSX_BUNDLE_GUI_IDENTIFIER ${BUNDLE_ID}
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${BUNDLE_ID})

eacp_set_gui_subsystem(Hub)
set_default_target_setting(Hub)
