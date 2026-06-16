#include "WindowDebugServer.h"

#include <eacp/Core/Threads/EventLoop.h>
#include <eacp/Core/Utils/Base64.h>
#include <eacp/Core/Utils/Logging.h>
#include <eacp/Graphics/Graphics/Keyboard.h>
#include <eacp/Graphics/View/View.h>
#include <eacp/Graphics/Window/Window.h>

#include <Miro/Miro.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <utility>

namespace eacp::Graphics::Remote
{

namespace
{

struct BusyReset
{
    bool& flag;

    ~BusyReset() { flag = false; }
};

Miro::JSON field(const Miro::JSON& object, const std::string& key)
{
    if (!object.isObject())
        return {};

    auto& obj = object.asObject();
    auto it = obj.find(key);
    return it != obj.end() ? it->second : Miro::JSON {};
}

std::string optionalString(const Miro::JSON& args,
                           const std::string& key,
                           const std::string& fallback = {})
{
    auto value = field(args, key);
    return value.isString() ? value.asString() : fallback;
}

std::string emptySchema()
{
    return R"({"type":"object","properties":{}})";
}

double numberOr(const Miro::JSON& args, const std::string& key, double fallback)
{
    auto value = field(args, key);
    return value.isNumber() ? value.asNumber() : fallback;
}

MouseButton buttonOf(const Miro::JSON& args)
{
    switch (static_cast<int>(numberOr(args, "button", 0)))
    {
        case 1:
            return MouseButton::Right;
        case 2:
            return MouseButton::Middle;
        default:
            return MouseButton::Left;
    }
}

std::string defaultRecordingDir(const WindowDebugServerOptions& options)
{
    if (!options.recordingDir.empty())
        return options.recordingDir;

    // current_path() is "/" for a LaunchServices ("open") launch, which
    // isn't writable — fall back to the temp dir so recordings always
    // land somewhere. Callers can still pass an absolute name to override.
    auto base = std::filesystem::current_path();
    if (base == base.root_path())
        base = std::filesystem::temp_directory_path();

    return (base / "test-results" / "recordings").string();
}

} // namespace

WindowDebugServer::WindowDebugServer(Graphics::Window& windowToUse,
                                     WindowDebugServerOptions options)
    : windowRef(windowToUse)
    , mcpServer("eacp-debug-server", "1.0.0")
    , recordingDir(defaultRecordingDir(options))
{
    registerWindowTools();

    mcpServer.setInstructions(
        "Drives a live eacp app window. Capture what it shows with "
        "screenshot; record a session to MP4 with start_recording / "
        "stop_recording (the composited window — page, GPU or native "
        "drawing). Tool calls pump the app's event loop — issue them one "
        "at a time.");
    mcpServer.attach(http);

    if (!http.listen(options.port))
        throw std::runtime_error("WindowDebugServer: failed to listen on port "
                                 + std::to_string(options.port));

    LOG("WindowDebugServer: MCP endpoint at http://127.0.0.1:"
        + std::to_string(http.boundPort()) + "/mcp");
}

WindowDebugServer::~WindowDebugServer()
{
    http.stop();
}

int WindowDebugServer::port() const
{
    return http.boundPort();
}

Graphics::Window& WindowDebugServer::window()
{
    return windowRef;
}

MCP::Server& WindowDebugServer::mcp()
{
    return mcpServer;
}

HTTP::Response WindowDebugServer::handleMcp(const HTTP::Request& request)
{
    return mcpServer.handle(request);
}

void WindowDebugServer::addTool(std::string name,
                                std::string description,
                                const std::string& schemaJson,
                                MCP::ToolHandler handler)
{
    auto guarded = [this, handler = std::move(handler)](const Miro::JSON& args)
    {
        if (busy)
            return MCP::toolError("another tool call is still executing; "
                                  "issue tool calls one at a time");

        busy = true;
        auto reset = BusyReset {busy};
        return handler(args);
    };

    mcpServer.addTool({std::move(name),
                       std::move(description),
                       Miro::Json::parse(schemaJson),
                       std::move(guarded)});
}

void WindowDebugServer::addExtension(OwningPointer<ServerExtension> extension)
{
    extension->registerTools(*this);
    extensions.add(std::move(extension));
}

void WindowDebugServer::registerWindowTools()
{
    addTool("screenshot",
            "Capture the app window as a PNG image — the composited window, "
            "so it captures whatever the app shows (page, GPU, native).",
            emptySchema(),
            [this](const Miro::JSON&)
            {
                auto error = std::string {};
                auto image = windowRef.captureImage(&error);
                if (!image)
                    return MCP::toolError(error.empty() ? "screenshot failed"
                                                        : error);

                auto png = image.toPng();

                auto result = MCP::ToolResult {};
                result.content.add(MCP::imageContent(
                    Base64::encode(std::span {png.data(),
                                              static_cast<std::size_t>(png.size())}),
                    "image/png"));
                result.content.add(
                    MCP::textContent(std::to_string(image.width()) + "x"
                                     + std::to_string(image.height()) + " PNG"));
                return result;
            });

    addTool(
        "start_recording",
        "Begin recording the app window to an MP4 (the composited window, "
        "so it captures whatever the app shows — page, GPU, native). "
        "Recording runs while you keep issuing tool calls; finish with "
        "stop_recording. macOS only; the window must be visible.",
        R"({"type":"object","properties":{
                "name":{"type":"string",
                    "description":"output file stem, default 'recording'"},
                "fps":{"type":"number","description":"frames per second, default 30"}}})",
        [this](const Miro::JSON& args)
        {
            auto name = optionalString(args, "name", "recording");
            auto path =
                (std::filesystem::path {recordingDir} / (name + ".mp4")).string();

            auto recOpts = ScreenRecorder::Options {};
            if (auto fps = field(args, "fps"); fps.isNumber())
                recOpts.frameRateHz = static_cast<int>(fps.asNumber());

            auto error = std::string {};
            if (!recorder.start(windowRef, path, recOpts, &error))
                return MCP::toolError(error);

            return MCP::toolText("recording to " + path);
        });

    addTool("stop_recording",
            "Finish the recording started by start_recording and return the "
            "MP4 file path.",
            emptySchema(),
            [this](const Miro::JSON&)
            {
                if (!recorder.isRecording())
                    return MCP::toolError("not recording");

                auto path = recorder.stop();

                auto ec = std::error_code {};
                auto size = std::filesystem::file_size(path, ec);
                auto note =
                    ec ? std::string {} : " (" + std::to_string(size) + " bytes)";
                return MCP::toolText("saved " + path + note);
            });

    registerInputTools();
}

void WindowDebugServer::registerInputTools()
{
    // Input attaches to the window too: these feed synthetic events through
    // the content View's *real* dispatch (View::dispatchMouseEvent — the
    // exact entry the native mouse uses), so they go through the same
    // hit-testing, handlesMouseEvents gate and drag capture a human does.
    // An agent can drive a GPU / native app — orbit a camera, drag a
    // slider, draw on a canvas — through the identical path, and a view the
    // user can't drive (one that didn't opt into mouse events) the agent
    // can't either. Coordinates are window points, top-left origin.
    // (WebView apps have the selector-based DOM tools for richer driving.)

    auto contentOr = [this]() -> View* { return windowRef.getContentView(); };

    addTool("mouse_move",
            "Move the mouse to (x, y) in the window (a mouseMoved event).",
            R"({"type":"object","properties":{
                "x":{"type":"number"},"y":{"type":"number"}},
                "required":["x","y"]})",
            [this, contentOr](const Miro::JSON& args)
            {
                auto* view = contentOr();
                if (!view)
                    return MCP::toolError("no content view to receive input");

                auto event = MouseEvent {};
                event.pos = {static_cast<float>(numberOr(args, "x", 0)),
                             static_cast<float>(numberOr(args, "y", 0))};
                event.type = MouseEventType::Moved;
                view->dispatchMouseEvent(event);
                return MCP::toolText("moved");
            });

    addTool("mouse_click",
            "Click at (x, y) in the window: a mouseDown + mouseUp pair on the "
            "content view.",
            R"({"type":"object","properties":{
                "x":{"type":"number"},"y":{"type":"number"},
                "button":{"type":"number","description":"0 left, 1 right, 2 middle"}},
                "required":["x","y"]})",
            [this, contentOr](const Miro::JSON& args)
            {
                auto* view = contentOr();
                if (!view)
                    return MCP::toolError("no content view to receive input");

                auto at = Point {static_cast<float>(numberOr(args, "x", 0)),
                                 static_cast<float>(numberOr(args, "y", 0))};
                auto button = buttonOf(args);

                auto down = MouseEvent {};
                down.pos = at;
                down.downPos = at;
                down.type = MouseEventType::Down;
                down.button = button;
                view->dispatchMouseEvent(down);

                auto up = down;
                up.type = MouseEventType::Up;
                view->dispatchMouseEvent(up);

                Threads::runEventLoopFor(std::chrono::milliseconds {16});
                return MCP::toolText("clicked");
            });

    addTool(
        "mouse_drag",
        "Drag within the window: press at (from_x, from_y), move to "
        "(to_x, to_y) over `steps` increments, release — sending "
        "mouseDown / mouseDragged (with per-step deltas) / mouseUp to the "
        "content view. Drives camera orbit, sliders, canvas drawing. The "
        "drag is spread over real time so it animates and records smoothly.",
        R"({"type":"object","properties":{
                "from_x":{"type":"number"},"from_y":{"type":"number"},
                "to_x":{"type":"number"},"to_y":{"type":"number"},
                "steps":{"type":"number","description":"increments, default 24"},
                "button":{"type":"number","description":"0 left, 1 right, 2 middle"}},
                "required":["from_x","from_y","to_x","to_y"]})",
        [this, contentOr](const Miro::JSON& args)
        {
            auto* view = contentOr();
            if (!view)
                return MCP::toolError("no content view to receive input");

            auto fx = numberOr(args, "from_x", 0);
            auto fy = numberOr(args, "from_y", 0);
            auto tx = numberOr(args, "to_x", 0);
            auto ty = numberOr(args, "to_y", 0);
            auto steps = std::max(1, static_cast<int>(numberOr(args, "steps", 24)));
            auto button = buttonOf(args);
            auto from = Point {static_cast<float>(fx), static_cast<float>(fy)};

            auto down = MouseEvent {};
            down.pos = from;
            down.downPos = from;
            down.type = MouseEventType::Down;
            down.button = button;
            view->dispatchMouseEvent(down);

            auto prev = from;
            for (auto i = 1; i <= steps; ++i)
            {
                auto t = static_cast<float>(i) / static_cast<float>(steps);
                auto cur = Point {static_cast<float>(fx + (tx - fx) * t),
                                  static_cast<float>(fy + (ty - fy) * t)};

                auto drag = MouseEvent {};
                drag.pos = cur;
                drag.downPos = from;
                drag.delta = {cur.x - prev.x, cur.y - prev.y};
                drag.type = MouseEventType::Dragged;
                drag.button = button;
                view->dispatchMouseEvent(drag);

                // Pump a frame so the change renders (and the recorder
                // catches it) rather than jumping straight to the end.
                Threads::runEventLoopFor(std::chrono::milliseconds {16});
                prev = cur;
            }

            auto up = MouseEvent {};
            up.pos = prev;
            up.downPos = from;
            up.type = MouseEventType::Up;
            up.button = button;
            view->dispatchMouseEvent(up);

            return MCP::toolText("dragged over " + std::to_string(steps) + " steps");
        });

    addTool("scroll",
            "Scroll wheel at (x, y) by (dx, dy) — a mouseWheel event whose "
            "delta carries the movement. Drives zoom / pan.",
            R"({"type":"object","properties":{
                "x":{"type":"number"},"y":{"type":"number"},
                "dx":{"type":"number"},"dy":{"type":"number"}},
                "required":["dy"]})",
            [this, contentOr](const Miro::JSON& args)
            {
                auto* view = contentOr();
                if (!view)
                    return MCP::toolError("no content view to receive input");

                auto event = MouseEvent {};
                event.pos = {static_cast<float>(numberOr(args, "x", 0)),
                             static_cast<float>(numberOr(args, "y", 0))};
                event.delta = {static_cast<float>(numberOr(args, "dx", 0)),
                               static_cast<float>(numberOr(args, "dy", 0))};
                event.type = MouseEventType::Wheel;
                view->dispatchMouseEvent(event);

                Threads::runEventLoopFor(std::chrono::milliseconds {16});
                return MCP::toolText("scrolled");
            });

    addTool("key",
            "Send a key to the content view: a keyDown + keyUp pair. `key` is "
            "the characters (e.g. \" \" for space, \"r\"); `code` is an "
            "optional virtual key code.",
            R"({"type":"object","properties":{
                "key":{"type":"string"},
                "code":{"type":"number"}},
                "required":["key"]})",
            [this, contentOr](const Miro::JSON& args)
            {
                auto* view = contentOr();
                if (!view)
                    return MCP::toolError("no content view to receive input");

                auto event = KeyEvent {};
                event.characters = optionalString(args, "key");
                event.charactersIgnoringModifiers = event.characters;
                event.keyCode =
                    static_cast<std::uint16_t>(numberOr(args, "code", 0));
                event.type = KeyEventType::Down;
                view->dispatchKeyEvent(event);

                auto up = event;
                up.type = KeyEventType::Up;
                view->dispatchKeyEvent(up);

                Threads::runEventLoopFor(std::chrono::milliseconds {16});
                return MCP::toolText("key sent");
            });
}

} // namespace eacp::Graphics::Remote
