# eacp::React — a declarative tier over the component tree

`eacp::UI` is JUCE-shaped: you subclass `Component`, hold your children as
members, place them by hand in `resized()` and draw them in `paint()`. That is a
good model for a widget and a poor model for a *screen*, because what changes is
the data, and every change has to be threaded into the tree by hand — a field to
hold it, a setter to change it, a `resized()` that knows the new arrangement, a
`repaint()` nobody forgot.

This module leaves the component tier exactly as it is and puts a declarative
layer above it. You write a function of your state that returns a description of
the interface, styled inline; the reconciler works out what that means for the
real component tree and applies the difference. Nothing here replaces
`UI::Component` — every node this mounts *is* one, painted and batched by the
same host.

```cpp
Element TaskRow(Task task, Action<int> onToggle, Action<int> onRemove)
{
    return component([task, onToggle, onRemove]
    {
        return Row {.css = "gap-3 px-3 py-2 items-center bg-panel rounded-md"}({
            Checkbox {.checked  = task.done,
                      .onChange = [onToggle, id = task.id](bool) { onToggle(id); }},

            Label {.text = task.text,
                   .css  = task.done ? "flex-1 text-dim italic" : "flex-1"},

            Button {.text    = "Remove",
                    .onClick = [onRemove, id = task.id] { onRemove(id); }},
        });
    });
}
```

## The four pieces

**Elements are values.** `Label {.text = "Hi"}` allocates a props struct and a
type tag and nothing else — no component, no window-server object. A screen's
worth of them is a few hundred bytes, built and thrown away on every render.
Containers are *called* with their children, which is the only punctuation this
has that JSX does not; leaves convert to an `Element` on their own, so they sit
straight in a child list.

**Components are lambdas.** React's props are our captures:

```cpp
Element Row(int index, Action<> onRemove)
{
    return component([index, onRemove] { ... });
}
```

That decision is the one the rest of the design falls out of. C++ cannot compare
two closures, so props diffing is unavailable — and also unnecessary, because a
re-render walks down to host elements and diffs *those*, and their props are
plain structs that compare fine. What you skip by not diffing closures is some
re-running of render functions, which is cheap; what you would gain by trying is
the whole apparatus of a props type per component. `component(deps(...), fn)`
opts into memoization where a subtree is expensive enough to say so.

**Hooks hold state across renders.** Storage is a vector on the mounted
instance, indexed by call order, which is why the rules of hooks are the same
rules here: same calls, same order, every render.

```cpp
auto [tasks, setTasks] = useState(Tasks {});
useEffect([&] { LOG("mounted"); }, deps());
```

`useState` returns exactly two members so the structured binding reads like the
original. The setter holds a weak token to its instance, so a callback that
outlives its component is inert rather than a dangling write. Also here:
`useEffect`, `useRef`, `useMemo`, `useContext` / `provide`.

**Styling is inline, in Tailwind's vocabulary.** One `css` string per element,
parsed once per distinct string for the life of the process and cached, so every
later render is a hash lookup.

```
layout    flex-row flex-col flex-N grow flex-none
spacing   gap-N p-N px-N py-N pt-N pr-N pb-N pl-N  m-N mx-N my-N mt-N ...
size      w-N h-N w-full h-full w-auto h-auto min-w-N max-w-N min-h-N max-h-N
place     justify-start|center|end|between|around
          items-start|center|end|stretch   self-start|center|end|stretch
paint     bg-COLOUR  border  border-N  border-COLOUR
          rounded rounded-sm|md|lg|xl|full rounded-N
text      text-COLOUR  text-xs|sm|base|lg|xl|2xl|3xl  text-left|center|right
          font-bold font-normal italic  accent-COLOUR
```

The spacing scale is Tailwind's — a number is four points, so `p-4` is sixteen
and `gap-2` is eight. Brackets escape it: `w-[137]`, `text-[15]`,
`bg-[#ff8800]`. `COLOUR` is a theme name — `base`, `panel`, `text`, `dim`,
`accent`, `outline`, `hover`, `pressed` — or `transparent`, `white`, `black`, or
a literal, any of them with an opacity suffix: `bg-panel/50`.

Conditional styling is a conditional string, exactly as it is in JSX:
`.css = done ? "flex-1 text-dim italic" : "flex-1"`.

## What a frame costs

Reconciling does not draw. It sets props on components and bounds on components,
and both already know what to do about it: `setBounds` on an unchanged rect does
nothing, a widget setter handed the value it already has does nothing, and
`Component::repaint` is the only thing that makes `paint()` run. So a state
change that moves one number repaints one component, in a tree of any size — the
property the component tier was built for, kept.

State updates are batched. A setter marks its instance dirty and posts one flush
to the event loop, so ten setters in one handler cost one render pass, one
layout and one frame. It also means a handler can safely destroy the component
it was called from, which an immediate re-render could not.

## Interop, in both directions

A `React::Root` *is* a `UI::ComponentHost`, so it goes into a window, a split
view or an embedded host wherever a `ComponentHost` goes. Below it, `Host<T>`
mounts any `UI::Component` subclass you already have into a declarative tree, so
an existing hand-written widget is a leaf like any other — styled and laid out
like any other, and what it does inside is its own business.

## What it does not do

No wrapping, no absolute positioning, no aspect ratios — the flex subset is what
a screen layout needs and stops there. Two shaped clips do not compose, media
queries do not exist, and neither do pseudo-classes: `hover:` is a `useState` and
a conditional string. No concurrent rendering, no suspense, no portals. Error
boundaries do not exist — a throwing render function throws.

## Where to look

`Apps/ReactLike` is the demo: a task list with keyed rows, a controlled text
field, local state, an effect, and a `Host<T>` leaf reporting what the frame
cost. `Tests/React` is the specification — class parsing, flex layout,
reconciliation identity, batching and effect ordering, none of which needs a
window or a GPU.
