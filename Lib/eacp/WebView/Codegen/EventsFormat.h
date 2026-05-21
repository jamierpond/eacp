#pragma once

#include "../WebView/EventRegistry.h"

#include <Miro/Miro.h>

#include <span>
#include <string>
#include <string_view>

namespace eacp::Graphics::Codegen
{

// Emits a TypeScript module declaring a `ServerEvents` interface that
// maps each registered bridge event to its typed payload:
//
//   import type * as T from './schema';
//   export interface ServerEvents
//   {
//       todos: T.TodoState;
//   }
//
// Consumers (transports, hook factories) parametrize over this map to
// keep `backend.on(name, payload => …)` typed without spelling each
// event out twice.
//
// typeRoots must contain a TypeNode for every event's payload type;
// the formatter reads only the qualifiedName fields and is happy to
// receive extra unused roots.
std::string formatEventsModule(std::span<Miro::TypeTree::TypeNode> typeRoots,
                               std::span<const EventEntry> events,
                               std::string_view baseName);

} // namespace eacp::Graphics::Codegen
