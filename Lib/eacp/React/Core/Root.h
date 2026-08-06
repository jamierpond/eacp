#pragma once

#include "Instance.h"

namespace eacp::React
{
// A component tree described by a function, hosted in one GPUView.
//
// It is a ComponentHost, so it goes wherever one goes -- a window's content, a
// pane of a native split, an embedded view in somebody else's application --
// and everything below it is ordinary UI::Components, painted and batched by
// the host exactly as a hand-written tree would be.
//
// The function is called once at construction and again whenever state below it
// changes. What comes back is a description; what happens to the real tree is
// the difference between that description and the last one.
class Root : public UI::ComponentHost
{
public:
    // `app` is the whole interface. It is a render function like any other, so
    // it may use hooks, and it is re-run when its own state changes -- which is
    // what makes a single top-level function a workable place to start.
    explicit Root(std::function<Element()> app);
    ~Root() override;

    // How the one child is placed in the window. The default stretches it both
    // ways, so an app whose root element says `{.flex = 1}` fills the window,
    // and one that does not is as tall as its content.
    void setRootStyle(const Style& style);

    void resized() override;

    // Applies every pending state change now, rather than on the loop turn they
    // were scheduled for. What a test calls; nothing in an application should
    // need it.
    void flush();

    // Re-runs the whole tree from the top and applies the difference.
    //
    // Nothing in an application needs this -- a setter schedules exactly the
    // re-render its own change calls for, which is the point. It is for the
    // case the tier cannot see: something outside it changed, and the render
    // functions read it.
    void renderNow();

    // How many render functions the last flush ran, and how many components the
    // last layout moved or resized. The figures that say what a state change
    // actually cost, next to the host's own count of what it repainted.
    int getLastRenderCount() const { return lastRenders; }
    int getMountedInstanceCount() const;

private:
    friend void Detail::stateChanged(Instance&);
    friend void Detail::scheduleEffect(EffectSlot&);

    OwningPointer<Instance>
        mount(const Element& element, Instance* parent, UI::Component* hostParent);

    void updateInstance(Instance& instance, const Element& next);
    void renderFunction(Instance& instance);

    void reconcileChildren(Instance& parent,
                           const Children& next,
                           UI::Component* hostParent);

    void unmount(Instance& instance);
    void clearSubtreeDirty(Instance& instance);

    void scheduleUpdate(Instance& instance);
    void addPendingEffect(Instance& instance, EffectSlot& slot);
    void scheduleFlush();

    // Puts every component back into the order its element is in. Only needed
    // when the shape of the tree changed: z-order among siblings is child
    // order, and a list that reordered has to say so.
    void restack(Instance& instance);

    void performLayout();
    void runPendingEffects();

    std::function<Element()> app;
    OwningPointer<Instance> tree;

    // The function instance holding the app's render, which is the one node
    // re-rendering the whole tree means re-rendering.
    Instance* appInstance = nullptr;

    // Instances waiting to re-render, held by liveness token rather than by
    // pointer: a component can be unmounted between the setter that dirtied it
    // and the flush that would have re-rendered it.
    Vector<std::shared_ptr<Instance*>> pendingUpdates;

    // Effects wait for the commit, and their instance can be gone by then --
    // an effect scheduled by a render that a later reconcile threw away. So
    // each carries the same liveness token a setter does.
    struct PendingEffect
    {
        std::shared_ptr<Instance*> token;
        EffectSlot* slot = nullptr;
    };

    Vector<PendingEffect> pendingEffects;

    std::shared_ptr<Root*> alive = std::make_shared<Root*>(this);

    Style rootStyle;

    bool flushScheduled = false;
    bool structureChanged = false;
    int lastRenders = 0;
};
} // namespace eacp::React
