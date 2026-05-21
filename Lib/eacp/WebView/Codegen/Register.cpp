// Static-init registration of eacp's WebView codegen formats with the
// Miro type-export runner.
//
// This TU lives in the eacp-webview-codegen OBJECT library. The
// library is spliced into ${TARGET}Schema_Codegen by
// eacp_add_webview_app, so the initializers below fire whenever an
// app's codegen executable launches.

#include "EventsFormat.h"
#include "HooksFormat.h"

#include "../WebView/EventRegistry.h"

#include <Miro/Miro.h>

namespace
{

using Miro::TypeExport::EntryList;
using Miro::TypeExport::Format;
using Miro::TypeExport::registerFormat;

[[maybe_unused]] const auto eventsFormat = registerFormat(Format {
    "events",
    ".events.ts",
    [](const EntryList& entries, std::string_view baseName)
    {
        auto trees = Miro::TypeExport::buildAllTypeTrees(entries);
        return eacp::Graphics::Codegen::formatEventsModule(
            std::span<Miro::TypeTree::TypeNode> {trees},
            eacp::Graphics::Detail::eventRegistry(),
            baseName);
    },
});

[[maybe_unused]] const auto hooksFormat = registerFormat(Format {
    "hooks",
    ".hooks.ts",
    [](const EntryList& entries, std::string_view baseName)
    {
        auto trees = Miro::TypeExport::buildAllTypeTrees(entries);
        return eacp::Graphics::Codegen::formatHooksModule(
            std::span<Miro::TypeTree::TypeNode> {trees},
            Miro::CommandExport::Detail::registry(),
            eacp::Graphics::Detail::eventRegistry(),
            baseName);
    },
});

} // namespace
