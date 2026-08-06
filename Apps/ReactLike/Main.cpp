#include <eacp/React/React.h>

// The same component tier the UI examples use, described instead of assembled.
//
// There is no subclass here, no member per widget, no resized() and no
// repaint(). There is a function of the state that says what the interface is,
// styled inline the way a class attribute styles an element, and the tier works
// out what that means for the real component tree -- which is still a tree of
// UI::Components, painted and batched by one GPUView exactly as a hand-written
// one would be.
//
// Watch the footer while you type. It reports how many components the window
// holds and how many the last frame actually painted: adding a task re-runs
// every render function on screen and paints the one row that changed.

using namespace eacp;
using namespace eacp::React;

namespace
{
struct Task
{
    int id = 0;
    std::string text;
    bool done = false;

    bool operator==(const Task&) const = default;
};

using Tasks = Vector<Task>;

enum class Filter
{
    All,
    Active,
    Done
};

bool matches(const Task& task, Filter filter)
{
    if (filter == Filter::Active)
        return !task.done;

    if (filter == Filter::Done)
        return task.done;

    return true;
}

int nextId(const Tasks& tasks)
{
    auto highest = 0;

    for (const auto& task: tasks)
        highest = std::max(highest, task.id);

    return highest + 1;
}

// A component: a function returning what to show. Its props are its arguments,
// and the lambda's captures are what carry them into the render.
Element TaskRow(Task task, Action<int> onToggle, Action<int> onRemove)
{
    return component(
        [task, onToggle, onRemove]
        {
            return Row {.css = "gap-3 px-3 py-2 items-center bg-panel rounded-md"}({
                Checkbox {
                    .checked = task.done,
                    .onChange = [onToggle, id = task.id](bool) { onToggle(id); },
                },
                Label {
                    .text = task.text,
                    .css = task.done ? "flex-1 text-dim italic" : "flex-1 text-text",
                },
                Button {
                    .text = "Remove",
                    .onClick = [onRemove, id = task.id] { onRemove(id); },
                },
            });
        });
}

// No state of its own, so no component() wrapper: a plain function returning an
// element is as much a part of the tree as one that holds hooks.
Element
    FilterTab(std::string text, Filter value, Filter current, Action<Filter> onPick)
{
    return Button {
        .text = std::move(text),
        .onClick = [onPick, value] { onPick(value); },
        .toggle = true,
        .on = value == current,
        .css = "w-18",
    };
}

// Local state, owned by the thing it belongs to and invisible above it. Nothing
// in the app knows this panel can be opened.
Element Notes()
{
    return component(
        []
        {
            auto [open, setOpen] = useState(false);

            return Column {.css = "gap-2"}({
                Button {
                    .text = open ? "Hide notes" : "Show notes",
                    .onClick = [open, setOpen] { setOpen(!open); },
                    .css = "self-start",
                },

                open ? Column {.css = "gap-1 p-3 bg-panel rounded-md"}({
                           Label {.text = "State lives in the hook, not a member.",
                                  .css = "text-dim text-sm"},
                           Label {.text = "Keys keep a row's state when the list "
                                          "reorders.",
                                  .css = "text-dim text-sm"},
                           Label {.text = "Nothing above this panel knows it opens.",
                                  .css = "text-dim text-sm"},
                       })
                     : nothing(),
            });
        });
}

// What the tier costs, read from the host at paint time. An ordinary
// UI::Component, mounted into the declarative tree by Host<T> -- the escape
// hatch, and the reason a widget you already have does not need rewriting to be
// used from here.
class Meter final : public UI::Component
{
public:
    void paint(UI::Graphics& g) override
    {
        auto* host = getHost();

        if (host == nullptr)
            return;

        auto text = std::to_string(host->getLastComponentCount()) + " components  "
                    + std::to_string(host->getLastPaintedComponentCount())
                    + " painted  " + std::to_string(host->getLastClipChangeCount())
                    + " batches";

        g.setColour(theme().dimText);
        g.drawText(text, getLocalBounds(), Justification::Right);

        // The figures are only complete once the walk that produced them is
        // over, so this paints the last frame's and asks for one more to catch
        // up. Self-limiting: once they stop changing, so does this.
        if (text == lastPainted)
            return;

        lastPainted = text;

        // Guarded, and that is the part worth copying. A bare `this` works in a
        // hand-written tree where a component lives as long as the window, and
        // is a dangling call here: unmounting is ordinary in a declarative one,
        // and a posted callback outliving the component that posted it is the
        // normal case rather than a strange one. Same shape the tier uses for
        // its own setters -- a token the callback can ask about, rather than a
        // pointer it has to trust.
        Threads::callAsync(
            [this, token = std::weak_ptr<bool> {alive}]
            {
                if (!token.expired())
                    repaint();
            });
    }

private:
    std::string lastPainted;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

Element App()
{
    return component(
        []
        {
            auto [tasks, setTasks] = useState(Tasks {
                {1, "Describe the interface as a value", true},
                {2, "Diff it against what is mounted", true},
                {3, "Place it with flex, not resized()", false},
                {4, "Repaint only what actually moved", false},
            });

            auto [draft, setDraft] = useState(std::string {});
            auto [filter, setFilter] = useState(Filter::All);

            auto remaining = 0;

            for (const auto& task: tasks)
                if (!task.done)
                    ++remaining;

            useEffect([count = tasks.size()]
                      { LOG("tasks: " + std::to_string(count)); },
                      deps(tasks.size()));

            auto add = [tasks, draft, setTasks, setDraft]
            {
                if (draft.empty())
                    return;

                auto next = tasks;
                next.add({nextId(tasks), draft, false});

                setTasks(next);
                setDraft(std::string {});
            };

            auto toggle = [tasks, setTasks](int id)
            {
                auto next = tasks;

                for (auto& task: next)
                    if (task.id == id)
                        task.done = !task.done;

                setTasks(next);
            };

            auto remove = [tasks, setTasks](int id)
            {
                auto next = Tasks {};

                for (const auto& task: tasks)
                    if (task.id != id)
                        next.add(task);

                setTasks(next);
            };

            auto rows = Children {};

            for (const auto& task: tasks)
                if (matches(task, filter))
                    rows.add(keyed(task.id, TaskRow(task, toggle, remove)));

            return Column {.css = "flex-1 gap-4 p-5 bg-base"}({
                Label {.text = "Tasks", .css = "text-2xl font-bold"},

                Row {.css = "gap-2"}({
                    Field {
                        .value = draft,
                        .placeholder = "What needs doing?",
                        .onChange = setDraft,
                        .onSubmit = [add](const std::string&) { add(); },
                        .css = "flex-1",
                    },
                    Button {.text = "Add", .onClick = add},
                }),

                Row {.css = "gap-2 items-center"}({
                    FilterTab("All", Filter::All, filter, setFilter),
                    FilterTab("Active", Filter::Active, filter, setFilter),
                    FilterTab("Done", Filter::Done, filter, setFilter),
                    Spacer {},
                    Label {.text = std::to_string(remaining) + " left",
                           .css = "text-dim text-sm"},
                }),

                Scroll {.css = "flex-1 gap-2 p-2 bg-black/20 rounded-lg"}(
                    std::move(rows)),

                Notes(),

                Host<Meter> {.preferred = {0.f, 16.f}, .css = "h-4"},
            });
        });
}

Graphics::WindowOptions makeOptions()
{
    auto options = Graphics::WindowOptions {};
    options.width = 720;
    options.height = 640;
    options.title = "eacp React — a declarative component tree";
    options.minWidth = 420;
    options.minHeight = 360;

    return options;
}

struct DemoApp
{
    DemoApp() { window.setContentView(root); }

    Root root {App};
    Graphics::Window window {makeOptions()};
};
} // namespace

int main()
{
    return eacp::Apps::run<DemoApp>();
}
