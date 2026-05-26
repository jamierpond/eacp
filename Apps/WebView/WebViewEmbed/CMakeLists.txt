eacp_add_webview_app(WebViewEmbed
        SOURCES     Main.cpp
        API         Api::ParametersApi
        API_HEADER  Types.h
        WEB_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/web
        BUNDLE_ID   com.eacp.webviewembed
        BUNDLE_NAME "WebView Embed"
        REACT)
