import { defineConfig } from '@playwright/test';

// Playwright is used purely as a test runner — no browser engine is
// involved. The spec drives the in-app WebView via the embedded HTTP
// RPC server (compiled in when EACP_WEBVIEW_ENABLE_TEST_SERVER=ON).
export default defineConfig({
    testDir: '.',
    timeout: 30_000,
    fullyParallel: true,
    forbidOnly: !!process.env['CI'],
    retries: process.env['CI'] ? 1 : 0,
    reporter: process.env['CI'] ? 'github' : 'list',
});
