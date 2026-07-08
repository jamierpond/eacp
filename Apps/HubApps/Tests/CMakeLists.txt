# In-process contract tests for the HTTP RPC layer. Registered here (rather
# than under the top-level Tests/) because they link the demo's HubRpc lib.
find_package(NanoTest REQUIRED)
enable_testing()

nano_add_executable(HubAppsRpcTests
        SOURCES RpcContractTests.cpp
        TARGETS HubRpc)
