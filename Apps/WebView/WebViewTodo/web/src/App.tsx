import { memo, useState } from 'react';
import { backend } from './generated/backend';
import { useTodoIds, useTodoItem } from './generated/hooks';
import { useTodoSummary } from './derived';

export default function App()
{
    console.log('render: App');
    const ids = useTodoIds();
    const [draft, setDraft] = useState('');

    const submitDraft = () =>
    {
        const text = draft.trim();
        if (text.length === 0) return;
        void backend.addTodo({ text });
        setDraft('');
    };

    return (
        <main>
            <header>
                <h1>todos</h1>
                <p className="sub">
                    State lives in C++ (<code>StateValue&lt;TodoState&gt;</code>) and is
                    broadcast over the WebView bridge. Every change you make round-trips
                    through a typed command and comes back as an event.
                </p>
            </header>

            <section className="card">
                <form
                    className="composer"
                    onSubmit={(event) =>
                    {
                        event.preventDefault();
                        submitDraft();
                    }}
                >
                    <input
                        data-testid="todo-input"
                        type="text"
                        placeholder="What needs to be done?"
                        value={draft}
                        autoFocus
                        onChange={(event) => setDraft(event.target.value)}
                    />
                    <button
                        data-testid="todo-add"
                        type="submit"
                        disabled={draft.trim().length === 0}>
                        Add
                    </button>
                </form>

                <ul className="list" data-testid="todo-list">
                    {ids.map((id) => <TodoRow key={id} id={id} />)}
                    {ids.length === 0 && (
                        <li className="empty" data-testid="todo-empty">
                            Nothing here yet — add a todo above.
                        </li>
                    )}
                </ul>

                <TodoFooter />
            </section>
        </main>
    );
}

const TodoRow = memo(function TodoRow({ id }: { id: number })
{
    console.log('render: TodoRow', id);
    const item = useTodoItem(id);
    const [editing, setEditing] = useState(false);
    const [draft, setDraft] = useState('');

    if (!item) return null;

    const beginEdit = () =>
    {
        setDraft(item.text);
        setEditing(true);
    };

    const commitEdit = () =>
    {
        const next = draft.trim();
        setEditing(false);
        if (next.length === 0) void backend.removeTodo({ id: item.id });
        else if (next !== item.text) void backend.editTodo({ id: item.id, text: next });
    };

    return (
        <li
            data-testid="todo-item"
            data-todo-id={item.id}
            className={item.completed ? 'item done' : 'item'}>
            <input
                data-testid="todo-toggle"
                type="checkbox"
                checked={item.completed}
                onChange={() => void backend.toggleTodo({ id: item.id })}
            />
            {editing ? (
                <input
                    data-testid="todo-edit"
                    className="edit"
                    type="text"
                    value={draft}
                    autoFocus
                    onChange={(event) => setDraft(event.target.value)}
                    onBlur={commitEdit}
                    onKeyDown={(event) =>
                    {
                        if (event.key === 'Enter') commitEdit();
                        else if (event.key === 'Escape')
                        {
                            setDraft(item.text);
                            setEditing(false);
                        }
                    }}
                />
            ) : (
                <span
                    data-testid="todo-text"
                    className="text"
                    onDoubleClick={beginEdit}>
                    {item.text}
                </span>
            )}
            <button
                data-testid="todo-remove"
                type="button"
                className="remove"
                aria-label="Remove"
                onClick={() => void backend.removeTodo({ id: item.id })}
            >
                ×
            </button>
        </li>
    );
});

function TodoFooter()
{
    console.log('render: TodoFooter');
    const summary = useTodoSummary();
    const remaining = summary.total - summary.completed;

    return (
        <footer className="footer">
            <span className="count" data-testid="todo-count">
                <span data-testid="todo-remaining">{remaining}</span> remaining ·{' '}
                <span data-testid="todo-completed">{summary.completed}</span> done
            </span>
            <button
                data-testid="todo-clear-completed"
                type="button"
                className="link"
                disabled={summary.completed === 0}
                onClick={() => void backend.clearCompleted()}
            >
                Clear completed
            </button>
        </footer>
    );
}
