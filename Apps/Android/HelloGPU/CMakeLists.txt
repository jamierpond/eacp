add_library(HelloGPU SHARED Main.cpp)
target_link_libraries(HelloGPU PRIVATE eacp-gpu eacp-text)
set_default_target_setting(HelloGPU)

eacp_add_android_apk(HelloGPU
        PACKAGE com.eacp.hellogpu
        LABEL "eacp HelloGPU")
