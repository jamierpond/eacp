#include "Types.h"

#include <eacp/WebView/WebView.h>

#include <ea_data_structures/Pointers/Broadcaster.h>

#include <string>
#include <utility>

namespace
{

std::string trim(const std::string& input)
{
    auto start = input.find_first_not_of(" \t\n");

    if (start == std::string::npos)
        return {};

    auto end = input.find_last_not_of(" \t\n");
    return input.substr(start, end - start + 1);
}

class TodoStore
{
public:
    TodoStore()
    {
        state.items.create(nextId++, "Try editing me (double-click)", false);
        state.items.create(nextId++, "Toggle a checkbox", false);
        state.items.create(nextId++, "Add a new todo above", true);
    }

    const TodoState& get() const { return state; }
    EA::Broadcaster& getBroadcaster() { return broadcaster; }

    void addTodo(const std::string& text)
    {
        auto trimmed = trim(text);

        if (trimmed.empty())
            return;

        state.items.create(nextId++, std::move(trimmed), false);
        broadcaster.trigger();
    }

    void toggleTodo(long long id)
    {
        for (auto& item: state.items)
        {
            if (item.id == id)
            {
                item.completed = !item.completed;
                broadcaster.trigger();
                return;
            }
        }
    }

    void editTodo(long long id, std::string text)
    {
        for (auto& item: state.items)
        {
            if (item.id == id)
            {
                item.text = std::move(text);
                broadcaster.trigger();
                return;
            }
        }
    }

    void removeTodo(long long id)
    {
        auto erased = state.items.eraseIf(
            [&](const TodoItem& item) { return item.id == id; });

        if (erased)
            broadcaster.trigger();
    }

    void clearCompleted()
    {
        auto erased = state.items.eraseIf(
            [](const TodoItem& item) { return item.completed; });

        if (erased)
            broadcaster.trigger();
    }

private:
    TodoState state;
    EA::Broadcaster broadcaster;
    long long nextId = 1;
};

} // namespace

TodoStore& todoStore()
{
    static auto store = TodoStore {};
    return store;
}

EACP_KEYED_STATE(TodoState, todoStore, todos, items, id)

TodoState getTodos()
{
    return todoStore().get();
}

void addTodo(const AddTodoRequest& req)
{
    todoStore().addTodo(req.text);
}

void toggleTodo(const TodoIdRequest& req)
{
    todoStore().toggleTodo(req.id);
}

void editTodo(const EditTodoRequest& req)
{
    todoStore().editTodo(req.id, req.text);
}

void removeTodo(const TodoIdRequest& req)
{
    todoStore().removeTodo(req.id);
}

void clearCompleted()
{
    todoStore().clearCompleted();
}

MIRO_EXPORT_COMMANDS(
    getTodos, addTodo, toggleTodo, editTodo, removeTodo, clearCompleted)
