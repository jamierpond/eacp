set(CMAKE_FOLDER Examples/HubApps)

# JSON / serialisation layer (Miro). The HTTP transport comes from
# eacp-network-rpc, linked by the Rpc library below.
find_package(Miro REQUIRED)

add_subdirectory(Hub)
add_subdirectory(SecretPremiumApp)

if (EACP_ENABLE_TESTS)
    add_subdirectory(Tests)
endif ()
