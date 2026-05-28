#import <Cocoa/Cocoa.h>

#include <NanoTest/NanoTest.h>

// Prebuilt main() for in-process WebView test binaries (linked via
// eacp-webview-test-main).
//
// WKWebView creation expects NSApplication to be initialized — eacp
// normally bootstraps that inside Threads::runEventLoopFor / Apps::run,
// but our tests construct WebView instances directly on the main
// thread before any runloop pump happens. Force-initialize the
// shared application early so the first TestApp<MyApp>{} doesn't
// race with NSApp bring-up.
//
// nano::run iterates registered tests on this thread; each test
// owns a TestApp<MyApp> and uses runEventLoopFor() internally to
// pump WebKit callbacks.
int main(int argc, char* argv[])
{
    [NSApplication sharedApplication];
    return nano::run(argc, argv);
}
