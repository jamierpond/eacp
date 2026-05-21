#pragma once

#include <Miro/Miro.h>

#include <string>
#include <vector>

struct TodoItem
{
    long long id = 0;
    std::string text;
    bool completed = false;

    MIRO_REFLECT(id, text, completed)
};

struct TodoState
{
    EA::Vector<TodoItem> items;

    MIRO_REFLECT(items)
};

struct AddTodoRequest
{
    std::string text;

    MIRO_REFLECT(text)
};

struct TodoIdRequest
{
    long long id = 0;

    MIRO_REFLECT(id)
};

struct EditTodoRequest
{
    long long id = 0;
    std::string text;

    MIRO_REFLECT(id, text)
};

TodoState getTodos();
void addTodo(const AddTodoRequest& req);
void toggleTodo(const TodoIdRequest& req);
void editTodo(const EditTodoRequest& req);
void removeTodo(const TodoIdRequest& req);
void clearCompleted();
