import { createRpcClient, type RpcClient } from './client.ts';

// Mirror of the C++ test.* command surface (see
// Lib/eacp/WebView/Test/TestHarness.cpp). Each method round-trips
// through one HTTP POST → bridge dispatch → evaluateJavaScript →
// window.__test.<op> call in the page.
//
// All methods reject the returned Promise on failure (selector not
// found, JS exception, RPC timeout). Use `exists` / `waitFor` to gate
// optional behaviour instead of catching.

export interface AppDriverOptions
{
    /** Default per-command timeout. C++ side defaults to 5000ms. */
    defaultTimeoutMs?: number;
}

export class AppDriver
{
    private readonly rpc: RpcClient;
    private readonly defaultTimeoutMs: number | undefined;

    constructor(rpcUrl: string, options: AppDriverOptions = {})
    {
        this.rpc = createRpcClient(rpcUrl);
        this.defaultTimeoutMs = options.defaultTimeoutMs;
    }

    get rpcUrl(): string { return this.rpc.baseUrl; }

    /**
     * Calls the underlying RPC bridge directly. Useful for hitting
     * application commands (the ones the app registers itself), not
     * just test.* helpers.
     */
    invoke<T = unknown>(command: string, payload?: unknown): Promise<T>
    {
        return this.rpc.invoke<T>(command, payload);
    }

    click(selector: string, options?: CallOptions): Promise<boolean>
    {
        return this.run('test.click', { selector, ...this.timeout(options) });
    }

    fill(selector: string, value: string, options?: CallOptions): Promise<boolean>
    {
        return this.run('test.fill', { selector, value, ...this.timeout(options) });
    }

    press(selector: string, key: string, options?: CallOptions): Promise<boolean>
    {
        return this.run('test.press', { selector, key, ...this.timeout(options) });
    }

    submit(selector: string, options?: CallOptions): Promise<boolean>
    {
        return this.run('test.submit', { selector, ...this.timeout(options) });
    }

    text(selector: string, options?: CallOptions): Promise<string>
    {
        return this.run('test.text', { selector, ...this.timeout(options) });
    }

    attr(selector: string, name: string, options?: CallOptions): Promise<string | null>
    {
        return this.run('test.attr', { selector, name, ...this.timeout(options) });
    }

    exists(selector: string, options?: CallOptions): Promise<boolean>
    {
        return this.run('test.exists', { selector, ...this.timeout(options) });
    }

    count(selector: string, options?: CallOptions): Promise<number>
    {
        return this.run('test.count', { selector, ...this.timeout(options) });
    }

    waitFor(selector: string, options?: CallOptions): Promise<boolean>
    {
        return this.run('test.waitFor', { selector, ...this.timeout(options) });
    }

    /**
     * Evaluates an arbitrary JS expression in the page. The expression
     * is wrapped in a function body, so it can reference window /
     * document / any in-page globals. The return value must be JSON-
     * serializable.
     */
    evaluate<T = unknown>(expression: string, options?: CallOptions): Promise<T>
    {
        return this.run('test.evaluate', { expression, ...this.timeout(options) });
    }

    private run<T>(command: string, payload: object): Promise<T>
    {
        return this.rpc.invoke<T>(command, payload);
    }

    private timeout(options?: CallOptions): { timeoutMs?: number }
    {
        const value = options?.timeoutMs ?? this.defaultTimeoutMs;
        return value !== undefined ? { timeoutMs: value } : {};
    }
}

export interface CallOptions
{
    /** Override per-call. C++ side caps at this many ms. */
    timeoutMs?: number;
}
