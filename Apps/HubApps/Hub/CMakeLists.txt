add_executable(Hub Main.cpp)
target_link_libraries(Hub PRIVATE eacp-core HubIpc)
set_default_target_setting(Hub)
