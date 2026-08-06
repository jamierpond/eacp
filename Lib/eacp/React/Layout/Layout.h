#pragma once

#include "../Core/Instance.h"

namespace eacp::React::Layout
{
// Places every component in the tree below `node`, which is assumed to have its
// own frame already.
//
// Two passes, the way every flex implementation is. The first measures bottom
// up: what each node would be if nothing constrained it -- a label's glyph run,
// a button's caption and padding, a column's children stacked. The second
// arranges top down, handing each node the space its parent decided it gets and
// distributing whatever is left over among the flexible ones.
//
// The instances with no component of their own -- function components and
// fragments -- are transparent to both. They are in the instance tree because
// hooks have to live somewhere, and they are not in the layout because a
// wrapper that changed a layout by existing would make refactoring a render
// function into two of them change the picture.
void perform(Instance& node, const Rect& frame);
} // namespace eacp::React::Layout
