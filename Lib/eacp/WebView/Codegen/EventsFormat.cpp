#include "EventsFormat.h"

#include <Miro/CommandExport/ResolvedTypes.h>

#include <sstream>

namespace eacp::Graphics::Codegen
{

std::string formatEventsModule(std::span<Miro::TypeTree::TypeNode> typeRoots,
                               std::span<const EventEntry> events,
                               std::string_view baseName)
{
    auto resolved = Miro::CommandExport::resolveTypes(typeRoots);

    auto out = std::ostringstream {};

    if (!events.empty())
        out << "import type * as T from './" << baseName << "';\n\n";

    out << "export interface ServerEvents";

    if (events.empty())
    {
        out << " {}\n";
        return out.str();
    }

    out << "\n{\n";

    for (auto& event: events)
    {
        auto payloadName =
            resolved.nameFor(event.payloadQualifiedName, event.payloadTypeName);
        out << "    " << event.name << ": T." << payloadName << ";\n";
    }

    out << "}\n";

    return out.str();
}

} // namespace eacp::Graphics::Codegen
