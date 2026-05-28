#include "App.h"

#include <eacp/WebView/Test/TestApp.h>

#include <NanoTest/NanoTest.h>

#include <Miro/Miro.h>

#include <string>

using namespace nano;
using namespace eacp::WebView::Test;

namespace
{

constexpr auto inputSelector = R"([data-testid="todo-input"])";
constexpr auto addSelector = R"([data-testid="todo-add"])";
constexpr auto itemSelector = R"([data-testid="todo-item"])";
constexpr auto textSelector = R"([data-testid="todo-text"])";
constexpr auto toggleSelector = R"([data-testid="todo-toggle"])";
constexpr auto removeSelector = R"([data-testid="todo-remove"])";
constexpr auto remainingSelector = R"([data-testid="todo-remaining"])";

std::string lastItemDescendant(const std::string& child)
{
    return std::string {itemSelector} + ":last-child " + child;
}

std::string firstItemDescendant(const std::string& child)
{
    return std::string {itemSelector} + ":first-child " + child;
}

} // namespace

auto tSeedsThreeTodos = test("WebViewTodo/seedsThreeTodosOnStartup") = []
{
    auto app = TestApp<MyApp> {};
    app.driver().waitFor(inputSelector);
    check(app.driver().count(itemSelector) == 3);
};

auto tAddsNewTodo = test("WebViewTodo/addsNewTodoViaForm") = []
{
    auto app = TestApp<MyApp> {};
    app.driver().waitFor(inputSelector);

    auto before = app.driver().count(itemSelector);

    app.driver().fill(inputSelector, "Buy milk");
    app.driver().click(addSelector);

    check(app.driver().count(itemSelector) == before + 1);
    check(app.driver().text(lastItemDescendant(textSelector)) == "Buy milk");
};

auto tToggleFlipsCompletion =
    test("WebViewTodo/toggleFlipsCompletionAndUpdatesFooter") = []
{
    auto app = TestApp<MyApp> {};
    app.driver().waitFor(inputSelector);

    auto before = std::stoi(app.driver().text(remainingSelector));

    app.driver().click(firstItemDescendant(toggleSelector));

    auto remaining = std::stoi(app.driver().text(remainingSelector));
    check(remaining == before - 1);
};

auto tRemovingTodo = test("WebViewTodo/removingTodoDecrementsCount") = []
{
    auto app = TestApp<MyApp> {};
    app.driver().waitFor(inputSelector);

    auto before = app.driver().count(itemSelector);

    app.driver().click(firstItemDescendant(removeSelector));

    check(app.driver().count(itemSelector) == before - 1);
};

auto tDomainRpcsReachable =
    test("WebViewTodo/domainRpcsReachableThroughSameBridge") = []
{
    auto app = TestApp<MyApp> {};
    app.driver().waitFor(inputSelector);

    // The bridge is shared with WebViewBridge, so the production
    // commands the React app calls (addTodo / getTodos) are also
    // reachable from the harness — handy for setting up state
    // without going through the UI.
    auto before = app.driver().invoke<TodoState>("getTodos");

    auto req = AddTodoRequest {};
    req.text = "Direct add via bridge";
    app.driver().invoke("addTodo", Miro::toJSON(req));

    auto after = app.driver().invoke<TodoState>("getTodos");

    check(after.items.size() == before.items.size() + 1);
    check(after.items[after.items.size() - 1].text == "Direct add via bridge");
};
