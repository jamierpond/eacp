import { spawn, type ChildProcess } from 'node:child_process';
import { once } from 'node:events';
import { setTimeout as delay } from 'node:timers/promises';

import { AppDriver, type AppDriverOptions } from './AppDriver.ts';

export interface LaunchOptions extends AppDriverOptions
{
    /**
     * Path to the app executable (built with
     * EACP_WEBVIEW_ENABLE_TEST_SERVER=ON), e.g.
     *   build/Apps/WebView/WebViewTodo/WebViewTodo.app/Contents/MacOS/WebViewTodo
     * The launcher does not look inside .app bundles; pass the inner
     * Mach-O directly. If omitted, falls back to the EACP_APP_BINARY
     * env var.
     *
     * Ignored in attach mode (when attachUrl / EACP_RPC_URL is set).
     */
    bundle?: string;

    /**
     * Attach to an already-running app instead of spawning one. The
     * URL should be the full RPC endpoint, e.g.
     * `http://127.0.0.1:8765/rpc`. If omitted, falls back to the
     * EACP_RPC_URL env var. When set, `close()` becomes a no-op so
     * the manually-launched app keeps running between test runs.
     */
    attachUrl?: string;

    /** Extra args to pass to the spawned child process. */
    args?: readonly string[];

    /** Env vars to pass through. process.env is inherited by default. */
    env?: Readonly<Record<string, string>>;

    /** Cwd for the spawned child process. Defaults to dirname(bundle). */
    cwd?: string;

    /**
     * Max time to wait for the EACP_RPC_PORT=<n> line on stdout. The
     * macOS first-launch path (entitlements + signature checks) can
     * take a beat — 10s is plenty in steady state.
     */
    startupTimeoutMs?: number;

    /** Pipe child stdout/stderr to this writer (e.g. process.stderr). */
    logger?: { write(chunk: string): void };
}

export interface LaunchedApp
{
    readonly driver: AppDriver;
    /** null in attach mode (no child was spawned). */
    readonly process: ChildProcess | null;
    readonly rpcUrl: string;
    close(): Promise<void>;
}

const portLineRegex = /EACP_RPC_PORT=(\d+)/;

// Well-known default — agreed with TestServer.cpp's defaultRpcPort.
// Lets `./MyApp` + `npm test` connect with zero env vars.
const defaultAttachUrl = 'http://127.0.0.1:8765/rpc';

export async function launchApp(options: LaunchOptions = {}): Promise<LaunchedApp>
{
    // Explicit bundle (or EACP_APP_BINARY) → spawn. Otherwise attach
    // — either to the URL the caller set, or to the well-known
    // default port if nothing was set at all.
    const wantsSpawn = options.bundle !== undefined
                       || process.env['EACP_APP_BINARY'] !== undefined;
    if (wantsSpawn)
        return await spawnAndLaunch(options);

    const attachUrl = options.attachUrl
                      ?? process.env['EACP_RPC_URL']
                      ?? defaultAttachUrl;
    return await attach(attachUrl, options);
}

async function attach(rpcUrl: string,
                      options: LaunchOptions): Promise<LaunchedApp>
{
    const driver = new AppDriver(rpcUrl, {
        defaultTimeoutMs: options.defaultTimeoutMs,
    });

    // Health probe — confirms the app is reachable up front so the
    // first test isn't the one to discover a typo'd URL.
    await driver.evaluate('1');

    return {
        driver,
        process: null,
        rpcUrl,
        close: async () => {},
    };
}

async function spawnAndLaunch(options: LaunchOptions): Promise<LaunchedApp>
{
    const bundle = options.bundle ?? process.env['EACP_APP_BINARY'];
    if (!bundle)
    {
        throw new Error(
            'launchApp(): no bundle path. Either pass `bundle:` (or set '
            + 'EACP_APP_BINARY) to spawn the app, or pass `attachUrl:` '
            + '(or set EACP_RPC_URL) to attach to a manually-launched '
            + 'app. The app must be built with '
            + 'EACP_WEBVIEW_ENABLE_TEST_SERVER=ON.');
    }

    const args = [...(options.args ?? [])];

    // Override EACP_RPC_PORT=0 so each spawned child takes an
    // ephemeral port — the app's built-in default (8765) is meant
    // for solo manual launches, and would collide across parallel
    // test workers. We discover the actual port from the child's
    // stdout (EACP_RPC_PORT=<n> line) regardless of what we asked for.
    const childEnv: Record<string, string | undefined> = {
        ...process.env,
        EACP_RPC_PORT: '0',
        ...options.env,
    };

    const child = spawn(bundle, args, {
        cwd: options.cwd,
        env: childEnv as NodeJS.ProcessEnv,
        stdio: ['ignore', 'pipe', 'pipe'],
    });

    const teardown = async (): Promise<void> =>
    {
        if (child.exitCode !== null || child.signalCode !== null) return;
        child.kill('SIGTERM');
        // Cleanup race: if the child ignores SIGTERM the test would
        // hang. Give it a beat, then escalate.
        const exited = once(child, 'exit').then(() => true);
        const timeout = delay(2000).then(() => false);
        const ok = await Promise.race([exited, timeout]);
        if (!ok) child.kill('SIGKILL');
    };

    try
    {
        const rpcPort = await waitForPort(child, options.startupTimeoutMs ?? 10_000,
                                          options.logger);
        const rpcUrl = `http://127.0.0.1:${rpcPort}/rpc`;

        const driver = new AppDriver(rpcUrl, {
            defaultTimeoutMs: options.defaultTimeoutMs,
        });

        // Health probe — the port line means listen() returned, but the
        // dispatcher thread pool might not have a worker on hand yet
        // for the very first request. One round-trip warms it up.
        await driver.evaluate('1');

        return {
            driver,
            process: child,
            rpcUrl,
            close: teardown,
        };
    }
    catch (err)
    {
        await teardown();
        throw err;
    }
}

async function waitForPort(child: ChildProcess, timeoutMs: number,
                           logger?: { write(chunk: string): void }): Promise<number>
{
    let stdoutBuffer = '';
    let stderrBuffer = '';

    return await new Promise<number>((resolve, reject) =>
    {
        const onStdout = (chunk: Buffer): void =>
        {
            const text = chunk.toString('utf8');
            logger?.write(text);
            stdoutBuffer += text;

            const match = portLineRegex.exec(stdoutBuffer);
            if (match && match[1])
            {
                cleanup();
                resolve(Number.parseInt(match[1], 10));
            }
        };

        const onStderr = (chunk: Buffer): void =>
        {
            const text = chunk.toString('utf8');
            logger?.write(text);
            stderrBuffer += text;
        };

        const onExit = (code: number | null, signal: NodeJS.Signals | null): void =>
        {
            cleanup();
            reject(new Error(`app exited before announcing port `
                             + `(code=${code}, signal=${signal})\n`
                             + `stdout: ${stdoutBuffer}\nstderr: ${stderrBuffer}`));
        };

        const onTimeout = (): void =>
        {
            cleanup();
            reject(new Error(`timed out after ${timeoutMs}ms waiting for `
                             + `EACP_RPC_PORT=<n> on stdout.\n`
                             + `stdout so far: ${stdoutBuffer}\n`
                             + `stderr so far: ${stderrBuffer}`));
        };

        const timer = setTimeout(onTimeout, timeoutMs);

        const cleanup = (): void =>
        {
            clearTimeout(timer);
            child.stdout?.off('data', onStdout);
            child.stderr?.off('data', onStderr);
            child.off('exit', onExit);
        };

        child.stdout?.on('data', onStdout);
        child.stderr?.on('data', onStderr);
        child.on('exit', onExit);
    });
}
