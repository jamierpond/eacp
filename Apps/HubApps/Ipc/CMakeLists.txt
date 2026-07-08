# Shared nng transport for the Hub demo. Uses Miro for the JSON /
# serialisation layer and nng as the byte transport.
add_library(HubIpc STATIC NngRpc.cpp)

target_include_directories(HubIpc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Miro (JSON + reflection) comes in via the interface lib; nng is the wire.
target_link_libraries(HubIpc PUBLIC Miro nng)

set_default_target_setting(HubIpc)
