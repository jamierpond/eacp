import type * as T from './schema';

export type Handlers = {
    getCurrentTick(): T.Tick | Promise<T.Tick>;
};

export class UnknownCommandError extends Error
{
    httpStatus = 404;
    constructor(command: string)
    {
        super(`Unknown command: ${command}`);
    }
}

export async function dispatch(handlers: Handlers, command: string, _payload: unknown): Promise<unknown>
{
    switch (command)
    {
        case 'getCurrentTick': return await handlers.getCurrentTick();
        default: throw new UnknownCommandError(command);
    }
}
