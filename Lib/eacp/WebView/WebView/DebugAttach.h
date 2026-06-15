#pragma once

#include <eacp/Core/Utils/Containers.h>
#include <eacp/Core/Utils/Singleton.h>
#include <eacp/Graphics/Helpers/DebugAttach.h>

#include <functional>

namespace Miro
{
class Bridge;
}

namespace eacp::Graphics
{

class WebView;

namespace Detail
{

// The DebugAttachment base + the Window-level hook live in
// Graphics/Helpers/DebugAttach.h (capture is window-level, not WebView-
// specific). This adds the WebView-level hook: a factory keyed on a
// WebView + bridge pair, which the debug transport uses to contribute
// DOM-driving tools to the app's window server.
using DebugAttachFactory =
    std::function<OwningPointer<DebugAttachment>(WebView&, Miro::Bridge&)>;

// Empty until a debug transport installs itself — AutoAttachRegister.cpp
// does, when the app is built with the debug server enabled.
// WebViewBridge consults this at construction; in the unified design the
// returned attachment is usually empty (the window server owns the DOM
// tools), but the hook keeps the per-bridge ownership point for
// flexibility.
inline DebugAttachFactory& debugAttachFactory()
{
    return Singleton::get<DebugAttachFactory>();
}

} // namespace Detail

} // namespace eacp::Graphics
