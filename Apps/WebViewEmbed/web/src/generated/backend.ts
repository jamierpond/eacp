import { makeBackend } from './schema.backend';
import { makeBridge, type Transport } from './schema.bridge';

interface EacpBridge
{
    invoke<Res = unknown, Req = unknown>(command: string, payload?: Req): Promise<Res>;
    on<T = unknown>(event: string, handler: (payload: T) => void): () => void;
}

declare global
{
    interface Window
    {
        eacp: EacpBridge;
    }
}

const webViewTransport: Transport = {
    invoke: (command, payload) => window.eacp.invoke(command, payload),
    on:     (event, handler) =>
        window.eacp.on(event, handler as (payload: unknown) => void),
};

export const backend = makeBridge(webViewTransport, makeBackend);
