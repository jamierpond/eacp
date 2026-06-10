#include <eacp/WebView/WebView.h>
#include <eacp/Core/Platform/Platform.h>

using namespace eacp;
using namespace Graphics;

// Self-contained page demonstrating window dragging. The title bar is marked
// `--eacp-app-region: drag` (the marker eacp's injected window-drag.js reads;
// `-webkit-app-region` is kept alongside for Electron parity), so pressing and
// dragging it moves this frameless window. The button is `no-drag`, proving
// controls stay clickable and don't move the window.
//
// The WebView opts into acceptFirstMouse, so the drag works even when the
// window is in the background: the click that activates the window also
// reaches the page, instead of needing one click to focus and a second to
// drag. Switch to another app and drag this window's title bar to see it.
static const char* kDemoHtml = R"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  html, body { margin: 0; height: 100%;
               font-family: -apple-system, system-ui, sans-serif; }
  body { background: #0b0b0d; color: #fff; display: flex; flex-direction: column; }

  .titlebar {
    -webkit-app-region: drag;
    --eacp-app-region: drag;
    height: 52px;
    display: flex;
    align-items: center;
    gap: 12px;
    padding-left: 88px;   /* clear the traffic lights */
    background: linear-gradient(90deg, #1b1b22, #101014);
    border-bottom: 1px solid #26262e;
    user-select: none;
  }
  .titlebar .title { font-weight: 600; font-size: 13px; opacity: .9; }
  .spacer { flex: 1; }
  /* No traffic lights on Windows: the title sits flush left, and the
     standard caption buttons render flush right. */
  body.win .titlebar { padding-left: 16px; }
  body:not(.win) .titlebar { padding-right: 16px; }

  /* The standard Windows caption cluster (min / max / close). Hidden on
     macOS, where the native traffic lights are the controls. Contiguous
     buttons, so it sits outside the titlebar's flex gap. */
  .controls { display: none; height: 52px; margin-left: 4px; }
  body.win .controls { display: flex; }

  .winctl {
    -webkit-app-region: no-drag;
    --eacp-app-region: no-drag;
    width: 46px; height: 52px;
    border: none; border-radius: 0; background: transparent;
    color: #b9b9c4; padding: 0; cursor: default;
    font-family: "Segoe MDL2 Assets"; font-size: 10px;
  }
  .winctl:hover { background: #2a2a34; color: #fff; }
  .winctl.close:hover { background: #e81123; color: #fff; }

  button {
    -webkit-app-region: no-drag;
    --eacp-app-region: no-drag;
    border: 1px solid #34343f; background: #1e1e26; color: #fff;
    height: 30px; padding: 0 14px; border-radius: 8px;
    font-size: 12px; cursor: pointer;
  }
  button:hover { background: #2a2a34; }

  .content { flex: 1; display: grid; place-items: center;
             text-align: center; padding: 24px; }
  .content h1 { font-size: 20px; margin: 0 0 8px; }
  .content p { margin: 4px 0; color: #b9b9c4; font-size: 13px; max-width: 460px; }
  code { color: #7e89ff; }
  .badge { margin-top: 16px; font-size: 12px; color: #3df1a6; }
</style>
</head>
<body>
  <div class="titlebar">
    <span class="title">&#x283F; Drag this bar to move the window</span>
    <span class="spacer"></span>
    <button id="ping">Click me (no-drag)</button>
    <div class="controls">
      <button id="min" class="winctl" title="Minimize">&#xE921;</button>
      <button id="max" class="winctl" title="Maximize">&#xE922;</button>
      <button id="close" class="winctl close" title="Close">&#xE8BB;</button>
    </div>
  </div>
  <div class="content">
    <div>
      <h1>eacp window-drag demo</h1>
      <p>The bar above is <code>--eacp-app-region: drag</code> &mdash; press and
         drag it to move this frameless window.</p>
      <p>The button is <code>no-drag</code>, so it stays clickable and never
         moves the window.</p>
      <p>Thanks to <code>acceptFirstMouse</code>, this works even from the
         background &mdash; focus another app, then drag the bar in one
         gesture.</p>
      <div class="badge" id="status">ready</div>
    </div>
  </div>
  <script>
    if (navigator.userAgent.indexOf('Windows') !== -1)
      document.body.classList.add('win');

    var n = 0;
    document.getElementById('ping').addEventListener('click', function () {
      n += 1;
      document.getElementById('status').textContent =
        'button clicked ' + n + 'x — still clickable, not dragging';
    });

    function postToNative(name) {
      var handlers = window.webkit && window.webkit.messageHandlers;
      if (handlers && handlers[name]) handlers[name].postMessage('');
    }

    document.getElementById('min').addEventListener('click', function () {
      postToNative('__fancyMinimize');
    });

    var maximized = false;
    document.getElementById('max').addEventListener('click', function () {
      maximized = !maximized;
      this.innerHTML = maximized ? '&#xE923;' : '&#xE922;'; /* restore glyph */
      this.title = maximized ? 'Restore' : 'Maximize';
      postToNative('__fancyMaximize');
    });

    document.getElementById('close').addEventListener('click', function () {
      postToNative('__fancyClose');
    });
  </script>
</body>
</html>
)HTML";

struct RootView final : View
{
    RootView() { addChildren({webView}); }

    void resized() override { scaleToFit({webView}); }

    // acceptFirstMouse lets the title-bar drag start from an unfocused
    // window in a single gesture (see the page comment above).
    static WebView::Options getWebViewOptions()
    {
        auto options = WebView::Options();
        options.acceptFirstMouse = true;
        return options;
    }

    WebView webView {getWebViewOptions()};
};

struct MyApp
{
    MyApp()
    {
        rootView.webView.addScriptMessageHandler(
            "__fancyMinimize", [this](const std::string&) { window.minimize(); });
        rootView.webView.addScriptMessageHandler("__fancyMaximize",
                                                 [this](const std::string&)
                                                 { window.toggleMaximize(); });
        rootView.webView.addScriptMessageHandler(
            "__fancyClose", [](const std::string&) { Apps::quit(); });

        rootView.webView.loadHTML(kDemoHtml);
        window.setContentView(rootView);
    }

    // Mirrors the Electron window's titleBarStyle: 'hidden' + backgroundColor
    // + trafficLightPosition so the native window's chrome matches the web
    // app. A FullSizeContentView with a transparent, separator-less titlebar
    // lets the web app's own header render under the traffic lights as one
    // seamless black bar.
    //
    // Windows has no chrome to integrate with, so there the demo is a
    // frameless rounded window whose web title bar IS the chrome: drag
    // region, demo button, and the integrated close control. Resizable keeps
    // the invisible frame's edge band live, so the window still resizes.
    static WindowOptions getOptions()
    {
        auto options = WindowOptions();

        options.width = 1200;
        options.height = 800;
        options.minWidth = 1200;
        options.minHeight = 800;

        if (Platform::isWindows())
        {
            options.flags = {WindowFlags::Borderless, WindowFlags::Resizable};
            options.cornerRadius = 12.f;
            return options;
        }

        options.flags.emplace_back(WindowFlags::FullSizeContentView);
        options.showTitle = false;

        options.titlebarTransparent = true;
        options.showTitlebarSeparator = false;
        options.trafficLightPosition = Point {10.f, 11.f};
        options.backgroundColor = Color::black();

        return options;
    }

    RootView rootView;
    Window window {getOptions()};
};

int main()
{
    eacp::Apps::run<MyApp>();
    return 0;
}
