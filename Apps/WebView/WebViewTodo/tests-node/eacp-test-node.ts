// Shim — re-exports the eacp-test-node framework runtime alongside
// the Playwright runner surface so spec files only need one import.
//
// Living outside node_modules means Playwright's TS transformer
// handles both this file and the framework source it points at;
// `@playwright/test` resolves from THIS dir's node_modules instead of
// from the framework's real-path ancestry (which has none).
export * from '../../../../tools/eacp-test-node/src/index.ts';
export { test, expect } from '@playwright/test';
