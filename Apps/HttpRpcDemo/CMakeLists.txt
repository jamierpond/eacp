add_executable(HttpRpcDemo Main.cpp)
target_link_libraries(HttpRpcDemo PRIVATE eacp-network-rpc)

target_include_directories(HttpRpcDemo PRIVATE
        ${WEBVIEWEMBED_CPP_GENERATED_DIR})

add_dependencies(HttpRpcDemo WebViewEmbedCppSchema)

set_default_target_setting(HttpRpcDemo)
