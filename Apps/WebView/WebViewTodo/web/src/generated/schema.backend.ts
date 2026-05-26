import type * as T from './schema';

export type Invoke = (command: string, payload: unknown) => Promise<unknown>;

export function makeBackend(invoke: Invoke)
{
    return {
        getTodos: (): Promise<T.TodoState> =>
            invoke('getTodos', {}) as Promise<T.TodoState>,
        addTodo: (req: T.AddTodoRequest): Promise<void> =>
            invoke('addTodo', req) as Promise<void>,
        toggleTodo: (req: T.TodoIdRequest): Promise<void> =>
            invoke('toggleTodo', req) as Promise<void>,
        editTodo: (req: T.EditTodoRequest): Promise<void> =>
            invoke('editTodo', req) as Promise<void>,
        removeTodo: (req: T.TodoIdRequest): Promise<void> =>
            invoke('removeTodo', req) as Promise<void>,
        clearCompleted: (): Promise<void> =>
            invoke('clearCompleted', {}) as Promise<void>,
    };
}
