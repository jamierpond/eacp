// Generated. Do not edit by hand.
//
// React-friendly bindings over the eacp WebView bridge. Two layers:
//
//   - Low-level hooks (useNativeEvent / useNativeState) take an
//     explicit backend, event name and command name on every call.
//   - High-level factories (makeNativeEvent / makeNativeState) close
//     over those values once at module scope and return a typed custom
//     hook so consumer components look like:
//         const [params, setParams] = useParameters();

import { useEffect, useState, useSyncExternalStore } from 'react';

export interface EventCapableBackend
{
    on?: (event: string, handler: (payload: unknown) => void) => () => void;
}

// ---------- low-level hooks ----------

export function useNativeEvent<T>(
    backend: EventCapableBackend,
    eventName: string,
    initial: T,
): T
{
    const [value, setValue] = useState<T>(initial);

    useEffect(() => backend.on?.(eventName, (payload) =>
        setValue(payload as T)), [backend, eventName]);

    return value;
}

export function useNativeState<T>(
    backend: EventCapableBackend,
    eventName: string,
    setCommand: (req: T) => Promise<void>,
    initial: T,
): [T, (next: T) => void]
{
    const [value, setValue] = useState<T>(initial);

    useEffect(() => backend.on?.(eventName, (payload) =>
        setValue(payload as T)), [backend, eventName]);

    const set = (next: T): void =>
    {
        setValue(next);
        void setCommand(next).catch(
            (err) => console.error('useNativeState: setCommand failed', err));
    };

    return [value, set];
}

// ---------- module-scope factories ----------

export interface NativeEventConfig<T>
{
    backend: EventCapableBackend;
    event: string;
    initial: T;
}

// Builds a custom hook bound to one event source. Call at module
// scope; export the result as a `use*` named hook.
//
//   export const useTick = makeNativeEvent<Tick>({
//       backend, event: 'tick', initial: { angle: 0 },
//   });
//
//   function Component() { const tick = useTick(); ... }
export function makeNativeEvent<T>(config: NativeEventConfig<T>): () => T
{
    const { backend, event, initial } = config;

    return function useEvent(): T
    {
        return useNativeEvent(backend, event, initial);
    };
}

export interface NativeStateConfig<T>
{
    backend: EventCapableBackend;
    event: string;
    setCommand: (req: T) => Promise<void>;
    initial: T;
}

// Builds a custom hook bound to one bidirectional state binding.
// The setter is a typed command reference (e.g. backend.setParameters),
// not a string, so typos are caught at compile time. Call at module
// scope; export the result as a `use*` named hook.
//
//   export const useParameters = makeNativeState<Parameters>({
//       backend,
//       event: 'parameters',
//       setCommand: backend.setParameters,
//       initial: { level: 0.5, autoCycle: false, counter: 0 },
//   });
//
//   function Component()
//   {
//       const [params, setParams] = useParameters();
//       ...
//   }
export function makeNativeState<T>(
    config: NativeStateConfig<T>,
): () => [T, (next: T) => void]
{
    const { backend, event, setCommand, initial } = config;

    return function useState(): [T, (next: T) => void]
    {
        return useNativeState(backend, event, setCommand, initial);
    };
}

// ---------- External-store factory ----------
//
// Bridges a C++-owned state value into a React `useSyncExternalStore`
// hook. Compared with makeNativeState:
//
//   - Concurrent-mode safe: getSnapshot is read on every render, so
//     React can never tear against the live store.
//   - Initial fetch is built in: `fetch` is invoked once at module load
//     so the first render has real data instead of waiting for the next
//     C++ broadcast.
//   - No setter is baked in. Action-style commands (add/toggle/remove)
//     don't fit the "one set" shape; call typed commands on `backend`
//     directly from event handlers.
//
// Selector hooks (re-render only on the slice you read) can be layered
// on top by hand-writing a store with a Map-by-id + identity-preserving
// apply step and exposing per-slice `useSyncExternalStore` hooks.
//
//   export const useParameters = makeBridgeStore<Parameters>({
//       backend,
//       event: 'parameters',
//       fetch: backend.getParameters,
//       initial: { level: 0.5, autoCycle: false, counter: 0 },
//   });
//
//   function Component() { const params = useParameters(); ... }
export interface BridgeStoreConfig<T>
{
    backend: EventCapableBackend;
    event: string;
    fetch: () => Promise<T>;
    initial: T;
}

export function makeBridgeStore<T>(config: BridgeStoreConfig<T>): () => T
{
    let snapshot: T = config.initial;
    const listeners = new Set<() => void>();

    const setSnapshot = (next: T): void =>
    {
        snapshot = next;
        for (const listener of listeners) listener();
    };

    const subscribe = (listener: () => void): (() => void) =>
    {
        listeners.add(listener);
        return () => { listeners.delete(listener); };
    };

    const getSnapshot = (): T => snapshot;

    void config.fetch().then(setSnapshot).catch(
        (err) => console.error('makeBridgeStore: initial fetch failed', err));

    config.backend.on?.(config.event, (payload) => setSnapshot(payload as T));

    return function useStore(): T
    {
        return useSyncExternalStore(subscribe, getSnapshot);
    };
}
