add_library(stb_truetype STATIC stb_truetype.c)
target_include_directories(stb_truetype PUBLIC ${CMAKE_CURRENT_LIST_DIR})

silence_target_warnings(stb_truetype)
eacp_force_optimization(stb_truetype)
