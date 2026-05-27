export { createRpcClient, RpcError, type RpcClient } from './client.ts';
export { AppDriver, type AppDriverOptions, type CallOptions } from './AppDriver.ts';
export { launchApp, type LaunchOptions, type LaunchedApp } from './launch.ts';

// Note: the Playwright runner surface (`test`, `expect`,
// `defineConfig`) is intentionally NOT re-exported here. This
// module is consumed from outside its own directory tree, so
// re-exporting `@playwright/test` would force Node to resolve it
// from the framework's real path (which has no node_modules).
// Consumers re-export both this module and `@playwright/test`
// from a small consumer-side shim — see the WebViewTodo app's
// tests-node/eacp-test-node.ts for an example.
