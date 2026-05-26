export { createRpcClient, RpcError, type RpcClient } from './client.ts';
export { AppDriver, type AppDriverOptions, type CallOptions } from './AppDriver.ts';
export { launchApp, type LaunchOptions, type LaunchedApp } from './launch.ts';

// Re-export the Playwright runner surface so app-side spec files can
// pull everything from one place. Specs live in app dirs that don't
// have their own node_modules, so importing @playwright/test directly
// from a spec would fail Node's resolver — going through this barrel
// resolves @playwright/test against the framework's node_modules.
export { test, expect } from '@playwright/test';
