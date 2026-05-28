#include <NanoTest/NanoTest.h>

// Prebuilt main() for in-process WebView test binaries (linked via
// eacp-webview-test-main).
//
// Windows doesn't have a global "shared application" concept like
// NSApp that we need to pre-initialize. WebView2 spins up its own
// COM apartment lazily on first use. nano::run iterates tests on
// this thread; each test pumps the Win32 message loop via
// runEventLoopFor() for WebView2 callbacks.
int main(int argc, char* argv[])
{
    return nano::run(argc, argv);
}
