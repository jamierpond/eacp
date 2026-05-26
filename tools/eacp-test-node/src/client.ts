// Thin POST /rpc client. Mirrors the wire protocol used by both
// eacp::HTTP::Rpc::Server (envelope = {command, payload}, response =
// {result} or {error}) and the WebView script-message bridge.
//
// Intentionally untyped at this layer — the AppDriver layer above
// wraps individual commands with typed signatures.

export class RpcError extends Error
{
    constructor(message: string, readonly status: number, readonly command: string)
    {
        super(`RPC ${command} failed (${status}): ${message}`);
        this.name = 'RpcError';
    }
}

export interface RpcClient
{
    invoke<T = unknown>(command: string, payload?: unknown): Promise<T>;
    readonly baseUrl: string;
}

export function createRpcClient(baseUrl: string): RpcClient
{
    const url = baseUrl.replace(/\/+$/, '');

    return {
        baseUrl: url,
        async invoke<T = unknown>(command: string, payload: unknown = null): Promise<T>
        {
            const res = await fetch(url, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ command, payload }),
            });

            const body = (await res.json().catch(() => ({}))) as
                { result?: unknown; error?: string };

            if (!res.ok)
                throw new RpcError(body.error ?? `HTTP ${res.status}`, res.status, command);

            return body.result as T;
        },
    };
}
