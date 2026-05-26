import type * as T from './schema';

export type Invoke = (command: string, payload: unknown) => Promise<unknown>;

export function makeBackend(invoke: Invoke)
{
    return {
        getCurrentTick: (): Promise<T.Tick> =>
            invoke('getCurrentTick', {}) as Promise<T.Tick>,
    };
}
