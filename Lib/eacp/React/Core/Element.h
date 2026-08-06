#pragma once

#include "../Layout/Style.h"

#include <eacp/Core/Utils/Containers.h>

#include <functional>
#include <memory>
#include <string>

namespace eacp::React
{
class Instance;

// What a hook compares to decide whether to run again.
//
// `always` rather than an empty-vs-absent list, because a default argument
// cannot express "no array was written" the way JavaScript's absent argument
// can. So useEffect(fn) runs every render, useEffect(fn, deps()) runs once, and
// useEffect(fn, deps(a, b)) runs when either changes.
struct Deps
{
    bool always = true;
    Vector<std::size_t> values;

    bool operator==(const Deps& other) const
    {
        if (always || other.always)
            return false;

        return values == other.values;
    }
};

template <typename... Args>
Deps deps(const Args&... args)
{
    auto result = Deps {};
    result.always = false;
    result.values = {std::hash<Args> {}(args)...};

    return result;
}

// The per-type behaviour a host element needs: how to build the component, how
// to push props onto it, what its class string means, and what size it would
// like to be.
//
// A table of function pointers rather than a virtual base, because the thing
// being described is a *type* and not an object -- there is one of these per
// element in the whole process, its address is that element's identity during
// reconciliation, and an Element carries a pointer to it for free.
struct HostType
{
    const char* name = "";
    UI::Component* (*create)() = nullptr;

    // `previous` is null on the first apply, and otherwise the props this
    // component last saw -- so a setter is only called when its value actually
    // changed, which is what keeps a re-render from repainting the tree.
    void (*apply)(UI::Component&,
                  const void* props,
                  const void* previous,
                  const Style&) = nullptr;

    Style (*styleOf)(const void*) = nullptr;

    // What this element would be if nothing constrained it: a label's glyph
    // run, a button's caption plus its padding. Zero for a container, whose
    // size comes from its children.
    Size (*measure)(const UI::Component&, const void*, const Style&) = nullptr;
};

// Specialize for each element the module exposes. A specialization is looked up
// when hostTypeFor is instantiated rather than when it is defined, which is
// what lets the generic machinery live here and the elements live next door.
template <typename WidgetType, typename PropsType>
struct HostTraits;

template <typename WidgetType, typename PropsType>
const HostType* hostTypeFor()
{
    using Traits = HostTraits<WidgetType, PropsType>;

    static const HostType type {
        Traits::name,

        [] { return static_cast<UI::Component*>(new WidgetType()); },

        [](UI::Component& component,
           const void* props,
           const void* previous,
           const Style& style)
        {
            Traits::apply(static_cast<WidgetType&>(component),
                          *static_cast<const PropsType*>(props),
                          static_cast<const PropsType*>(previous),
                          style);
        },

        [](const void* props)
        { return Traits::style(*static_cast<const PropsType*>(props)); },

        [](const UI::Component& component, const void* props, const Style& style)
        {
            return Traits::measure(static_cast<const WidgetType&>(component),
                                   *static_cast<const PropsType*>(props),
                                   style);
        }};

    return &type;
}

struct Element;
using Children = Vector<Element>;

// A description of a node, and never the node itself. Built on every render and
// thrown away, so it holds shared pointers to immutable props rather than
// anything a component owns.
struct Element
{
    enum class Kind
    {
        Host,
        Function,
        Fragment
    };

    Kind kind = Kind::Fragment;

    // What decides whether a re-render can keep the instance that is here or
    // has to replace it. The HostType for a host element, and the address of a
    // per-closure-type static for a function one -- so two different lambdas in
    // the two arms of a conditional do not silently inherit each other's hooks.
    const void* typeTag = nullptr;

    // Identity among siblings, for a list that reorders. Empty means "the one
    // in this position", which is right for everything that does not.
    std::string key;

    std::shared_ptr<const void> props;

    std::function<Element()> render;

    // Set by component(deps(...), fn): unchanged deps mean the subtree below is
    // left exactly as it is, render function included.
    Deps memo;
    bool memoized = false;

    // A provider is a fragment that carries a value, found by useContext
    // walking up the instance tree.
    const void* contextTag = nullptr;
    std::shared_ptr<const void> contextValue;

    Children children;
};

// Renders to nothing at all. What the empty arm of a conditional returns:
//     open ? Notes() : nothing()
inline Element nothing()
{
    return {};
}

// Several elements where one is expected, with no box around them. They are
// laid out by whatever box is above, exactly as if they had been written there.
inline Element fragment(Children children)
{
    auto element = Element {};
    element.children = std::move(children);

    return element;
}

// Names a node so it keeps its state when its siblings reorder, move or are
// removed from the middle. Without one, children match by position -- which is
// right for a layout and wrong for a list.
inline Element keyed(std::string key, Element element)
{
    element.key = std::move(key);

    return element;
}

inline Element keyed(int key, Element element)
{
    return keyed(std::to_string(key), std::move(element));
}

// A component: a callable of no arguments returning what to show. Its props are
// its captures, which is what makes a plain lambda enough.
//
// The static tag is per closure *type*, so every distinct lambda in the program
// has an identity the reconciler can compare, and two renders of the same
// lambda at the same position keep the same hooks.
template <typename Fn>
Element component(Fn function)
{
    static const char tag = 0;

    auto element = Element {};
    element.kind = Element::Kind::Function;
    element.typeTag = &tag;
    element.render = std::move(function);

    return element;
}

// The same, re-run only when `memoDeps` changes. For a subtree whose render is
// expensive enough to be worth saying so, and unnecessary otherwise -- a
// re-render that reaches unchanged props already costs nothing.
template <typename Fn>
Element component(Deps memoDeps, Fn function)
{
    auto element = component(std::move(function));
    element.memo = std::move(memoDeps);
    element.memoized = true;

    return element;
}

template <typename T>
const void* contextTagFor()
{
    static const char tag = 0;

    return &tag;
}

// Makes `value` visible to useContext<T>() anywhere below, without threading it
// through every render function in between.
template <typename T>
Element provide(T value, Children children)
{
    auto element = Element {};
    element.contextTag = contextTagFor<T>();
    element.typeTag = element.contextTag;
    element.contextValue = std::make_shared<const T>(std::move(value));
    element.children = std::move(children);

    return element;
}

template <typename WidgetType, typename PropsType>
Element makeHost(PropsType props, Children children = {})
{
    auto element = Element {};
    element.kind = Element::Kind::Host;
    element.typeTag = hostTypeFor<WidgetType, PropsType>();
    element.props = std::make_shared<const PropsType>(std::move(props));
    element.children = std::move(children);

    return element;
}

inline const HostType* hostTypeOf(const Element& element)
{
    return static_cast<const HostType*>(element.typeTag);
}
} // namespace eacp::React
