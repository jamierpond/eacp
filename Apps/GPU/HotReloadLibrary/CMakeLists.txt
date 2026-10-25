add_library(HotReloadLibraryViews STATIC Views.cpp Tuning.cpp Tuning.h DemoView.h)
target_link_libraries(HotReloadLibraryViews PUBLIC eacp-gpu)
target_include_directories(HotReloadLibraryViews PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_default_target_setting(HotReloadLibraryViews)

eacp_add_app(HotReloadLibrary Main.cpp)
target_link_libraries(HotReloadLibrary PRIVATE HotReloadLibraryViews)
set_target_properties(HotReloadLibrary PROPERTIES MACOSX_BUNDLE TRUE)
eacp_add_hot_reload(HotReloadLibrary MODULE HotReloadLibraryView
        LIBRARY HotReloadLibraryViews SOURCES Views.cpp Tuning.cpp)

eacp_set_gui_subsystem(HotReloadLibrary)
set_default_target_setting(HotReloadLibrary)
