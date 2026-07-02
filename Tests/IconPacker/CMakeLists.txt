nano_add_executable(IconPackerTests
        SOURCES IconPackerTests.cpp)

target_include_directories(IconPackerTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Tools")
