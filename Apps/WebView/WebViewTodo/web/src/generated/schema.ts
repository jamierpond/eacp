export interface TodoItem {
    id: number;
    text: string;
    completed: boolean;
}

export interface TodoState {
    items: TodoItem[];
}

export interface AddTodoRequest {
    text: string;
}

export interface TodoIdRequest {
    id: number;
}

export interface EditTodoRequest {
    id: number;
    text: string;
}

