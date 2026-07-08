# In-process contract tests for the HTTP Peer. Registered here (rather than
# under the top-level Tests/) because they link the demo's HubRpc library.
find_package(NanoTest REQUIRED)
enable_testing()

nano_add_executable(HubAppsPeerTests
        SOURCES PeerContractTests.cpp
        TARGETS HubRpc)
