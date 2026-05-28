#pragma once

#include <string>

namespace eacp::WebView::Test
{

// Returns the source of the window.__test DOM-driving agent that
// AppDriver expects to be installed in the page. TestApp<T> calls
// this and feeds the result to webView.addUserScript() at document-
// start, so the helpers are available before any page script runs.
//
// The agent itself lives at Resources/test-agent.js and is embedded
// into eacp-webview-test via res_embed_add().
std::string loadTestAgentSource();

} // namespace eacp::WebView::Test
