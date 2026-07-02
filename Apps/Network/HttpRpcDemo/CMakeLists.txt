miro_export(HttpRpcDemoSchema
        API         Api::PingApi
        API_HEADER  Schema.h
        OUTPUT_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/generated
        OUTPUT_NAME schema
        FORMATS     ts backend ts-server bridge)

miro_export_emit(HttpRpcDemoSchema
        OUTPUT_DIR  ${CMAKE_CURRENT_BINARY_DIR}/cpp-generated
        OUTPUT_NAME schema
        FORMATS     cpp-miro cpp-client)

# Server pulls in the API class via Schema.h alone — PingApi's methods
# are inline since the codegen executable also ODR-uses them.
add_executable(HttpRpcServer Server.cpp)
target_link_libraries(HttpRpcServer PRIVATE eacp-network-rpc)
eacp_target_uses_schema(HttpRpcServer HttpRpcDemoSchema)
set_default_target_setting(HttpRpcServer)

add_executable(HttpRpcClient Client.cpp)
target_link_libraries(HttpRpcClient PRIVATE eacp-network-rpc)
eacp_target_uses_schema(HttpRpcClient HttpRpcDemoSchema)
set_default_target_setting(HttpRpcClient)

eacp_set_icon(HttpRpcServer)
eacp_set_icon(HttpRpcClient)
