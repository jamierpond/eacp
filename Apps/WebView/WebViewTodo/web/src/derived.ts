import { useMemo } from 'react';
import { useTodos } from './generated/hooks';

export interface TodoSummary
{
    total: number;
    completed: number;
}

export function useTodoSummary(): TodoSummary
{
    const state = useTodos();

    return useMemo(() => ({
        total: state.items.length,
        completed: state.items.filter((i) => i.completed).length,
    }), [state]);
}
