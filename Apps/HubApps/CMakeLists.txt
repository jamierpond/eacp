set(CMAKE_FOLDER Examples/HubApps)

# JSON / serialisation layer (Miro) and the IPC transport (nng). Both are
# fetched via CPM the first time find_package runs; repeat calls are no-ops.
find_package(Miro REQUIRED)
find_package(nng REQUIRED)

add_subdirectory(Ipc)
add_subdirectory(Hub)
add_subdirectory(SecretPremiumApp)
