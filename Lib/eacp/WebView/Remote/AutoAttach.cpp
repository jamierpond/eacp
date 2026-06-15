#include "AutoAttach.h"

#include "WebViewTools.h"

#include <eacp/Core/Utils/Logging.h>
#include <eacp/Graphics/Remote/WindowAutoAttach.h>
#include <eacp/Graphics/Remote/WindowDebugServer.h>
#include <eacp/WebView/WebView/DebugAttach.h>

#include <utility>

namespace eacp::WebView::Remote
{

bool installAutoAttach()
{
    Graphics::Detail::debugAttachFactory() =
        [](Graphics::WebView& webView,
           Miro::Bridge& bridge) -> OwningPointer<Graphics::Detail::DebugAttachment>
    {
        try
        {
            // Hand the DOM tools to the app's window server — adopted now
            // if it exists, or the moment it's created (the Window and the
            // bridge can be constructed in either order). The window server
            // owns the tools, so the bridge keeps no attachment of its own.
            auto tools = OwningPointer<Graphics::Remote::ServerExtension> {};
            tools.create<WebViewTools>(webView, bridge);
            Graphics::Remote::attachExtensionOrDefer(std::move(tools));
        }
        catch (const std::exception& e)
        {
            // A broken debug affordance must never take the app down.
            LOG(std::string {"WebViewTools: failed to attach: "} + e.what());
        }

        return {};
    };

    return true;
}

} // namespace eacp::WebView::Remote
