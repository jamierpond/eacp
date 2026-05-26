import { defineConfig } from '@playwright/test';

// Playwright is used purely as a test runner here — none of the
// browser engines are involved. We still get parallelism, fixtures,
// retries, and the HTML report for free.
//
// EACP_PW_TEST_DIR is set by the CMake helper
// (eacp_add_webview_node_tests) to the app's tests-node/ directory so
// a single framework config can serve every app. When unset, no
// default test dir is configured — Playwright will require a path
// argument on the command line.
export default defineConfig({
    testDir: process.env['EACP_PW_TEST_DIR'] ?? '.',
    timeout: 30_000,
    fullyParallel: true,
    forbidOnly: !!process.env['CI'],
    retries: process.env['CI'] ? 1 : 0,
    reporter: process.env['CI'] ? 'github' : 'list',
});
