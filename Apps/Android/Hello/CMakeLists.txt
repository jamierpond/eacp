add_library(AndroidHello SHARED Main.cpp)
target_link_libraries(AndroidHello PRIVATE eacp-gpu eacp-text)
set_default_target_setting(AndroidHello)

eacp_add_android_apk(AndroidHello
        PACKAGE com.eacp.hello
        LABEL "eacp Hello")
