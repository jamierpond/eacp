#include <eacp/React/React.h>

#include <NanoTest/NanoTest.h>

// What the declarative tier promises, asked of it directly.
//
// Three separable claims, and a test file per part of each: a class string
// means one thing, a tree of those lays out where it says it does, and a
// re-render keeps the components it can rather than rebuilding the window.
//
// The last one is the one worth guarding. Everything the tier is for rests on a
// state change touching the components that changed and no others -- so the
// tests that matter are the ones counting what survived a render, not the ones
// checking that something appeared.

using namespace nano;
using namespace eacp;
using namespace eacp::React;

namespace
{
// A root laid out at a fixed size, with no window and no GPU. The layout pass
// touches neither: it measures through the glyph atlas and writes bounds.
struct Harness
{
    explicit Harness(std::function<Element()> app)
        : root(std::move(app))
    {
        root.setBounds({0.f, 0.f, 400.f, 300.f});
        root.flush();
    }

    // Something outside the tier changed and the render functions read it.
    void render() { root.renderNow(); }

    // A setter was called: apply what it scheduled and nothing else, which is
    // what the event loop would have done a turn later.
    void flush() { root.flush(); }

    Root root;
};

// The one box below the root, which is whatever the app returned.
UI::Component& firstChild(UI::Component& parent)
{
    return *parent.getChildren()[0];
}

} // namespace

auto t1Spacingscaleisfourpointsperunit =
    test("react.style/spacingScaleIsFourPointsPerUnit") = []
{
    auto style = parseStyle("gap-2 p-4 px-6");

    check(style.gap == 8.f);
    check(style.padding.top == 16.f);
    check(style.padding.left == 24.f);
    check(style.padding.right == 24.f);
};

auto t2Bracketsescapethescale = test("react.style/bracketsEscapeTheScale") = []
{
    auto style = parseStyle("w-[137] text-[15]");

    check(style.width.has_value());
    check(*style.width == 137.f);
    check(*style.fontSize == 15.f);
};

auto t3Latertokenswin = test("react.style/laterTokensWin") = []
{
    auto style = parseStyle("p-2 p-8");

    check(style.padding.top == 32.f);
};

auto t4Colournamesresolveagainstthetheme =
    test("react.style/colourNamesResolveAgainstTheTheme") = []
{
    auto style = parseStyle("bg-panel text-dim");

    check(style.background.r == theme().panel.r);
    check(style.textColour.has_value());
    check(style.textColour->g == theme().dimText.g);
};

auto t5Opacitysuffixscalesalpha = test("react.style/opacitySuffixScalesAlpha") = []
{
    auto style = parseStyle("bg-[#ff8800]/50");

    check(style.background.r == 1.f);
    check(style.background.a == 0.5f);
};

auto t6Borderwidthandcolouraretoldapartbyshape =
    test("react.style/borderWidthAndColourAreToldApartByShape") = []
{
    auto width = parseStyle("border-2");
    auto colour = parseStyle("border-accent");

    check(width.borderWidth == 2.f);

    // A colour on its own still draws, which is what "border-accent" plainly
    // means -- a width of zero would have been a silent no-op.
    check(colour.borderWidth == 1.f);
    check(colour.border.b == theme().accent.b);
};

auto t7Flexshareswhatisleftafterfixedchildren =
    test("react.layout/flexSharesWhatIsLeftAfterFixedChildren") = []
{
    auto harness = Harness {[]
                            {
                                return Row {.css = "flex-1 gap-0"}({
                                    Box {.css = "w-25"},
                                    Box {.css = "flex-1"},
                                });
                            }};

    auto& row = firstChild(*harness.root.getRootComponent());
    auto& fixed = *row.getChildren()[0];
    auto& flexible = *row.getChildren()[1];

    check(fixed.getWidth() == 100.f);
    check(flexible.getWidth() == 300.f);
    check(flexible.getBounds().x == 100.f);
};

auto t8Gapandpaddingcomeoutofthesamespace =
    test("react.layout/gapAndPaddingComeOutOfTheSameSpace") = []
{
    auto harness = Harness {[]
                            {
                                return Row {.css = "flex-1 gap-4 p-2"}({
                                    Box {.css = "flex-1"},
                                    Box {.css = "flex-1"},
                                });
                            }};

    auto& row = firstChild(*harness.root.getRootComponent());
    auto& first = *row.getChildren()[0];
    auto& second = *row.getChildren()[1];

    // 400 wide, 8 of padding a side, 16 of gap: 368 shared two ways.
    check(first.getWidth() == 184.f);
    check(first.getBounds().x == 8.f);
    check(second.getBounds().x == 208.f);
};

auto t9Afragmentisnotabox = test("react.layout/aFragmentIsNotABox") = []
{
    auto nested = Harness {[]
                           {
                               return Row {.css = "flex-1"}({
                                   fragment({
                                       Box {.css = "flex-1"},
                                       Box {.css = "flex-1"},
                                   }),
                               });
                           }};

    auto& row = firstChild(*nested.root.getRootComponent());

    // Both boxes are the row's children, and each got half of it -- a wrapper
    // that changed the layout by existing would make splitting a render
    // function in two change the picture.
    check(row.getChildren().size() == 2);
    check(row.getChildren()[0]->getWidth() == 200.f);
};

auto t10Alignselfoverridestherowsitems =
    test("react.layout/alignSelfOverridesTheRowsItems") = []
{
    auto harness = Harness {[]
                            {
                                return Row {.css = "flex-1 items-start"}({
                                    Box {.css = "flex-1 h-10 self-end"},
                                });
                            }};

    auto& row = firstChild(*harness.root.getRootComponent());
    auto& child = *row.getChildren()[0];

    check(child.getHeight() == 40.f);
    check(child.getBounds().y == 260.f);
};

auto t11Arerenderkeepsthecomponentsitcan =
    test("react.reconcile/aRerenderKeepsTheComponentsItCan") = []
{
    auto flag = std::make_shared<bool>(false);

    auto harness = Harness {[flag]
                            {
                                return Column {}({
                                    Label {.text = *flag ? "after" : "before"},
                                });
                            }};

    auto& column = firstChild(*harness.root.getRootComponent());
    auto* before = column.getChildren()[0];

    *flag = true;
    harness.render();

    // Same element type in the same position, so the label was updated rather
    // than destroyed and rebuilt -- which is what lets a widget keep the state
    // this tier knows nothing about, a caret position among it.
    check(column.getChildren()[0] == before);
};

auto t12Adifferentelementtypeisreplaced =
    test("react.reconcile/aDifferentElementTypeIsReplaced") = []
{
    auto flag = std::make_shared<bool>(false);

    auto harness = Harness {[flag]() -> Element
                            {
                                if (*flag)
                                    return Column {}({Button {.text = "x"}});

                                return Column {}({Label {.text = "x"}});
                            }};

    auto& column = firstChild(*harness.root.getRootComponent());
    auto* before = column.getChildren()[0];

    *flag = true;
    harness.render();

    check(column.getChildren()[0] != before);
};

auto t13Keyscarrystatethroughareorder =
    test("react.reconcile/keysCarryStateThroughAReorder") = []
{
    auto order = std::make_shared<Vector<int>>(Vector<int> {1, 2, 3});

    auto harness =
        Harness {[order]
                 {
                     auto rows = Children {};

                     for (auto id: *order)
                         rows.add(keyed(id, Label {.text = std::to_string(id)}));

                     return Column {}(std::move(rows));
                 }};

    auto& column = firstChild(*harness.root.getRootComponent());
    auto* second = column.getChildren()[1];

    *order = {3, 2, 1};
    harness.render();

    // The same component, now drawn in a different place. Without the key it
    // would have been the one that happened to be in position two, and every
    // row's widget state would have shifted by one.
    check(column.getChildren()[1] == second);
};

auto t14Unkeyedchildrenmatchbyposition =
    test("react.reconcile/unkeyedChildrenMatchByPosition") = []
{
    auto count = std::make_shared<int>(2);

    auto harness = Harness {[count]
                            {
                                auto rows = Children {};

                                for (auto index = 0; index < *count; ++index)
                                    rows.add(Label {.text = std::to_string(index)});

                                return Column {}(std::move(rows));
                            }};

    auto& column = firstChild(*harness.root.getRootComponent());
    auto* first = column.getChildren()[0];

    *count = 4;
    harness.render();

    check(column.getChildren().size() == 4);
    check(column.getChildren()[0] == first);
};

auto t15Asetterrerendersitsowncomponentonly =
    test("react.state/aSetterRerendersItsOwnComponentOnly") = []
{
    auto bump = std::make_shared<Action<>>([] {});
    auto outerRenders = std::make_shared<int>(0);
    auto innerRenders = std::make_shared<int>(0);

    auto harness =
        Harness {[bump, outerRenders, innerRenders]
                 {
                     ++*outerRenders;

                     return Column {}({
                         component(
                             [bump, innerRenders]
                             {
                                 ++*innerRenders;

                                 auto [count, setCount] = useState(0);
                                 *bump = [count, setCount] { setCount(count + 1); };

                                 return Label {.text = std::to_string(count)};
                             }),
                     });
                 }};

    auto outerBefore = *outerRenders;
    auto innerBefore = *innerRenders;

    (*bump)();
    harness.flush();

    check(*outerRenders == outerBefore);
    check(*innerRenders == innerBefore + 1);
};

auto t16Settingthevaluealreadythererendersnothing =
    test("react.state/settingTheValueAlreadyThereRendersNothing") = []
{
    auto set = std::make_shared<Action<>>([] {});
    auto renders = std::make_shared<int>(0);

    auto harness = Harness {[set, renders]
                            {
                                ++*renders;

                                auto [count, setCount] = useState(7);
                                *set = [setCount] { setCount(7); };

                                return Label {.text = std::to_string(count)};
                            }};

    auto before = *renders;

    (*set)();
    harness.flush();

    check(*renders == before);
};

auto t17Severalsettersinonehandlercostonerender =
    test("react.state/severalSettersInOneHandlerCostOneRender") = []
{
    auto both = std::make_shared<Action<>>([] {});
    auto renders = std::make_shared<int>(0);

    auto harness =
        Harness {[both, renders]
                 {
                     ++*renders;

                     auto [first, setFirst] = useState(0);
                     auto [second, setSecond] = useState(0);

                     *both = [first, second, setFirst, setSecond]
                     {
                         setFirst(first + 1);
                         setSecond(second + 1);
                     };

                     return Label {.text = std::to_string(first + second)};
                 }};

    auto before = *renders;

    (*both)();
    harness.flush();

    check(*renders == before + 1);
};

auto t18Asetterfiredafterunmountdoesnothing =
    test("react.state/aSetterFiredAfterUnmountDoesNothing") = []
{
    auto mounted = std::make_shared<bool>(true);
    auto set = std::make_shared<Action<>>([] {});

    auto harness =
        Harness {[mounted, set]() -> Element
                 {
                     if (!*mounted)
                         return Column {}({});

                     return Column {}({component(
                         [set]
                         {
                             auto [count, setCount] = useState(0);
                             *set = [count, setCount] { setCount(count + 1); };

                             return Label {.text = std::to_string(count)};
                         })});
                 }};

    *mounted = false;
    harness.render();

    // The component is gone and the callback outlived it, which is the ordinary
    // case rather than a strange one: a click can be the thing that removes the
    // row it was on.
    (*set)();
    harness.flush();

    check(true);
};

auto t19Runoncewithemptydependencies =
    test("react.effects/runOnceWithEmptyDependencies") = []
{
    auto runs = std::make_shared<int>(0);
    auto tick = std::make_shared<Action<>>([] {});

    auto harness = Harness {[runs, tick]
                            {
                                auto [count, setCount] = useState(0);
                                *tick = [count, setCount] { setCount(count + 1); };

                                useEffect([runs] { ++*runs; }, deps());

                                return Label {.text = std::to_string(count)};
                            }};

    check(*runs == 1);

    (*tick)();
    harness.flush();

    check(*runs == 1);
};

auto t20Rerunwhenadependencychanges =
    test("react.effects/rerunWhenADependencyChanges") = []
{
    auto runs = std::make_shared<int>(0);
    auto tick = std::make_shared<Action<>>([] {});

    auto harness = Harness {[runs, tick]
                            {
                                auto [count, setCount] = useState(0);
                                *tick = [count, setCount] { setCount(count + 1); };

                                useEffect([runs] { ++*runs; }, deps(count));

                                return Label {.text = std::to_string(count)};
                            }};

    check(*runs == 1);

    (*tick)();
    harness.flush();

    check(*runs == 2);
};

auto t21Cleanuprunsbeforethenextrunandatunmount =
    test("react.effects/cleanupRunsBeforeTheNextRunAndAtUnmount") = []
{
    auto cleanups = std::make_shared<int>(0);
    auto tick = std::make_shared<Action<>>([] {});

    {
        auto harness = Harness {
            [cleanups, tick]
            {
                auto [count, setCount] = useState(0);
                *tick = [count, setCount] { setCount(count + 1); };

                useEffect([cleanups] { return [cleanups] { ++*cleanups; }; },
                          deps(count));

                return Label {.text = std::to_string(count)};
            }};

        check(*cleanups == 0);

        (*tick)();
        harness.flush();

        check(*cleanups == 1);
    }

    check(*cleanups == 2);
};

auto t22Reachesdownwithoutbeingthreadedthrough =
    test("react.context/reachesDownWithoutBeingThreadedThrough") = []
{
    struct Accent
    {
        std::string name;
    };

    auto seen = std::make_shared<std::string>();

    auto harness = Harness {[seen]
                            {
                                return provide(Accent {"warm"},
                                               {Column {}({component(
                                                   [seen]
                                                   {
                                                       auto* accent =
                                                           useContext<Accent>();

                                                       if (accent != nullptr)
                                                           *seen = accent->name;

                                                       return nothing();
                                                   })})});
                            }};

    check(*seen == std::string {"warm"});
};

auto t23Unchangeddepsskiptherender =
    test("react.memo/unchangedDepsSkipTheRender") = []
{
    auto renders = std::make_shared<int>(0);
    auto tick = std::make_shared<Action<>>([] {});

    auto harness = Harness {[renders, tick]
                            {
                                auto [count, setCount] = useState(0);
                                *tick = [count, setCount] { setCount(count + 1); };

                                return Column {}({component(deps(0),
                                                            [renders]
                                                            {
                                                                ++*renders;
                                                                return nothing();
                                                            })});
                            }};

    auto before = *renders;

    (*tick)();
    harness.flush();

    check(*renders == before);
};
