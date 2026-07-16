# An app whose version differs from the enclosing tree declares its own
# project(): PROJECT_VERSION is scoped to the nearest project() call, so the
# version stamp (eacp_set_gui_subsystem) picks this one up.
project(Todo VERSION 2.5.0)

add_executable(Todo Main.cpp)
target_link_libraries(Todo PRIVATE eacp-graphics)

set_target_properties(Todo PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Todo App"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.eacp.todoapp")

eacp_set_gui_subsystem(Todo)
set_default_target_setting(Todo)
