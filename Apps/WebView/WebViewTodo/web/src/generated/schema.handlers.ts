import type * as T from './schema';

export type Handlers = {
    getTodos(): T.TodoState | Promise<T.TodoState>;
    addTodo(req: T.AddTodoRequest): void | Promise<void>;
    toggleTodo(req: T.TodoIdRequest): void | Promise<void>;
    editTodo(req: T.EditTodoRequest): void | Promise<void>;
    removeTodo(req: T.TodoIdRequest): void | Promise<void>;
    clearCompleted(): void | Promise<void>;
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
        case 'getTodos': return await handlers.getTodos();
        case 'addTodo': return await handlers.addTodo(payload as T.AddTodoRequest);
        case 'toggleTodo': return await handlers.toggleTodo(payload as T.TodoIdRequest);
        case 'editTodo': return await handlers.editTodo(payload as T.EditTodoRequest);
        case 'removeTodo': return await handlers.removeTodo(payload as T.TodoIdRequest);
        case 'clearCompleted': return await handlers.clearCompleted();
        default: throw new UnknownCommandError(command);
    }
}
