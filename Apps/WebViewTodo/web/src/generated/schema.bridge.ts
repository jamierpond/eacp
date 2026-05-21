// Transport-agnostic bridge runtime emitted by Miro. Pair with a
// transport adapter and a command factory (e.g. makeBackend from the
// matching .backend module) to get a typed client.
//
// Transport.invoke is required; Transport.on is optional, since
// request/response transports (plain HTTP) can't push events while
// duplex transports (WebView, WebSocket) can. The Bridge result type
// surfaces `on` with whatever shape the transport provided, so users
// on event-capable transports get full typing and users on
// request/response-only transports see `on` as undefined.

export type Invoke = (command: string, payload: unknown) => Promise<unknown>;
export type EventHandler = (payload: unknown) => void;
export type Unsubscribe = () => void;

export interface Transport
{
    invoke: Invoke;
    on?: (event: string, handler: EventHandler) => Unsubscribe;
}

export type Bridge<TBackend> = TBackend & { on: Transport['on'] };

export function makeBridge<TBackend>(
    transport: Transport,
    factory: (invoke: Invoke) => TBackend,
): Bridge<TBackend>
{
    const api = factory(transport.invoke.bind(transport)) as Bridge<TBackend>;
    api.on = transport.on?.bind(transport);
    return api;
}
