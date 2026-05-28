eacp_add_webview_test(WebViewTodoTests
        APP     WebViewTodo
        SOURCES WebViewTodoTests.cpp)

# The app's STATIC lib already propagates the source dir on its
# include path (so #include "App.h" works), so no extra
# target_include_directories needed.
