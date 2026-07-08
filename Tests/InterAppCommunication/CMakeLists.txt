nano_add_executable(IpcEndpointTests
        SOURCES EndpointTests.cpp
        TARGETS eacp-interapp)

nano_add_executable(IpcPeerTests
        SOURCES PeerTests.cpp
        TARGETS eacp-interapp)

nano_add_executable(IpcPollingClientTests
        SOURCES PollingClientTests.cpp
        TARGETS eacp-interapp)
