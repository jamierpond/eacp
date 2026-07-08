# A small, reusable inter-app communication layer over local HTTP RPC:
#   - Endpoint:      file-based rendezvous so a client can find a server's port
#   - Peer:          an HTTP server that serves a Miro API and can call peers
#   - PollingClient: a generic background client that polls a peer (header-only)
# Transport + JSON come transitively from eacp-network-rpc / Miro; nothing here
# is tied to a particular protocol.
add_library(eacp-interapp STATIC
        Endpoint.cpp
        Peer.cpp)

target_include_directories(eacp-interapp PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../../)
target_link_libraries(eacp-interapp PUBLIC eacp-network-rpc)

set_default_target_setting(eacp-interapp)
eacp_enable_unity_build(eacp-interapp)
add_ide_sources(eacp-interapp)
