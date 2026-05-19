import type * as T from './schema';

export type Invoke = (command: string, payload: unknown) => Promise<unknown>;

export function makeBackend(invoke: Invoke)
{
    return {
        getParameters: (): Promise<T.Parameters> =>
            invoke('getParameters', {}) as Promise<T.Parameters>,
        setParameters: (req: T.Parameters): Promise<void> =>
            invoke('setParameters', req) as Promise<void>,
    };
}
