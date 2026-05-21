#include "Types.h"

#include <eacp/WebView/WebView.h>

#include <utility>

namespace
{
long long nextTodoId = 1;

TodoState makeInitialState()
{
    auto state = TodoState {};
    state.items.create(nextTodoId++, "Try editing me (double-click)", false);
    state.items.create(nextTodoId++, "Toggle a checkbox", false);
    state.items.create(nextTodoId++, "Add a new todo above", true);
    return state;
}
} // namespace

EACP_KEYED_STATE(TodoState, todoState, todos, items, id, makeInitialState())

TodoState getTodos()
{
    return todoState().get();
}

void addTodo(const AddTodoRequest& req)
{
    auto trimmedStart = req.text.find_first_not_of(" \t\n");

    if (trimmedStart == std::string::npos)
        return;

    auto trimmedEnd = req.text.find_last_not_of(" \t\n");
    auto text = req.text.substr(trimmedStart, trimmedEnd - trimmedStart + 1);

    todoState().modify([&](TodoState& s)
                       { s.items.create(nextTodoId++, std::move(text), false); });
}

void toggleTodo(const TodoIdRequest& req)
{
    todoState().modify(
        [&](TodoState& s)
        {
            for (auto& item: s.items)
            {
                if (item.id == req.id)
                {
                    item.completed = !item.completed;
                    return;
                }
            }
        });
}

void editTodo(const EditTodoRequest& req)
{
    todoState().modify(
        [&](TodoState& s)
        {
            for (auto& item: s.items)
            {
                if (item.id == req.id)
                {
                    item.text = req.text;
                    return;
                }
            }
        });
}

void removeTodo(const TodoIdRequest& req)
{
    todoState().modify(
        [&](TodoState& s)
        {
            auto deleteFunc = [&](const TodoItem& item)
            { return item.id == req.id; };
            s.items.eraseIf(deleteFunc);
        });
}

void clearCompleted()
{
    todoState().modify(
        [](TodoState& s)
        {
            auto deleteFunc = [](const TodoItem& item) { return item.completed; };
            s.items.eraseIf(deleteFunc);
        });
}

MIRO_EXPORT_COMMANDS(
    getTodos, addTodo, toggleTodo, editTodo, removeTodo, clearCompleted)
