add_library(eacp-react STATIC)

# A declarative tier over the component tree: render functions, hooks,
# reconciliation and flex layout, producing ordinary UI::Components. Portable
# C++ over eacp-ui's public API, so there is nothing per-platform in it.
target_link_libraries(eacp-react PUBLIC eacp-ui)

target_sources(eacp-react PRIVATE
        Core/Hooks.cpp
        Core/Instance.cpp
        Core/Root.cpp
        Host/Box.cpp
        Host/Elements.cpp
        Layout/Layout.cpp
        Layout/Style.cpp)

set_default_target_setting(eacp-react)
eacp_enable_unity_build(eacp-react)
add_ide_sources(eacp-react)
