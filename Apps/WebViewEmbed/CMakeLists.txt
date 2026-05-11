eacp_add_webview_app(WebViewEmbed
        SOURCES         Main.cpp
        COMMAND_SOURCES Commands.cpp
        WEB_DIR         ${CMAKE_CURRENT_SOURCE_DIR}/web
        BUNDLE_ID       com.eacp.webviewembed
        BUNDLE_NAME     "WebView Embed")
