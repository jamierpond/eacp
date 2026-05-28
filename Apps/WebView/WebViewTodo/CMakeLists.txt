eacp_add_webview_app(WebViewTodo
        APP_HEADER  App.h
        SOURCES     Main.cpp
        API         Api::TodosApi
        API_HEADER  Types.h
        WEB_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/web
        CATEGORY    TodoApp
        BUNDLE_ID   com.eacp.webviewtodo
        BUNDLE_NAME "WebView Todo"
        REACT)

add_subdirectory(Tests)
