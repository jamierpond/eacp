import type * as T from './schema';

export type Handlers = {
    getParameters(): T.Parameters | Promise<T.Parameters>;
    setParameters(req: T.Parameters): void | Promise<void>;
};

export class UnknownCommandError extends Error
{
    httpStatus = 404;
    constructor(command: string)
    {
        super(`Unknown command: ${command}`);
    }
}

export async function dispatch(handlers: Handlers, command: string, payload: unknown): Promise<unknown>
{
    switch (command)
    {
        case 'getParameters': return await handlers.getParameters();
        case 'setParameters': return await handlers.setParameters(payload as T.Parameters);
        default: throw new UnknownCommandError(command);
    }
}
