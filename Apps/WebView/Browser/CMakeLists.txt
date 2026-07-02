add_executable(Browser Main.cpp)
target_link_libraries(Browser PRIVATE eacp-webview)

set(BUNDLE_ID "com.eacp.browser")

set_target_properties(Browser PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Browser"
        MACOSX_BUNDLE_GUI_IDENTIFIER ${BUNDLE_ID}
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${BUNDLE_ID})

eacp_set_gui_subsystem(Browser)
set_default_target_setting(Browser)

# The blue icon everywhere, except the Windows Alt-Tab switcher, which shows
# the orange one — demonstrating the WINDOWS_ALT_TAB_ICON override.
eacp_set_icon(Browser MAIN_ICON Icon.png WINDOWS_ALT_TAB_ICON AltTabIcon.png)
