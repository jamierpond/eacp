set(CMAKE_FOLDER ThirdParty)

add_subdirectory(miniz)

if (ANDROID)
    add_subdirectory(stb_truetype)
endif ()
