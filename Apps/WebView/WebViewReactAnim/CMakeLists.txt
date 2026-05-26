eacp_add_webview_app(WebViewReactAnim
        SOURCES     Main.cpp
        API         Api::Clock
        API_HEADER  Types.h
        WEB_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/web
        CATEGORY    ReactAnimApp
        BUNDLE_ID   com.eacp.webviewreactanim
        BUNDLE_NAME "WebView React Anim")
