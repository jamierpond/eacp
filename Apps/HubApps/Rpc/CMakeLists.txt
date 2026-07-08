# Bidirectional local-HTTP RPC peer, shared by the Hub and the app.
# eacp-network-rpc brings the HTTP server, the Miro RPC server/client, and
# Miro (the JSON / serialisation layer) transitively.
add_library(HubRpc STATIC
        RendezvousFile.cpp
        GatingServer.cpp
        GatingClient.cpp)

target_include_directories(HubRpc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(HubRpc PUBLIC eacp-network-rpc)

set_default_target_setting(HubRpc)
