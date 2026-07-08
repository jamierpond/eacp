add_executable(SecretPremiumApp Main.cpp)
target_link_libraries(SecretPremiumApp PRIVATE eacp-graphics HubRpc)

set(BUNDLE_ID "com.eacp.secretpremiumapp")

set_target_properties(SecretPremiumApp PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Secret Premium App"
        MACOSX_BUNDLE_GUI_IDENTIFIER ${BUNDLE_ID}
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${BUNDLE_ID})

eacp_set_gui_subsystem(SecretPremiumApp)
set_default_target_setting(SecretPremiumApp)
