#pragma once

#include <eacp/Core/Utils/Pimpl.h>
#include <eacp/Graphics/Image/Image.h>

#include <string>

namespace eacp::Graphics
{

class Window;
class View;

struct ScreenRecorderOptions
{
    // Captured/encoded frames per second.
    int frameRateHz = 30;
};

// Records the on-screen content of a window to an H.264 MP4 file.
//
// It captures the *composited* window image from the window server, so
// it records whatever the window actually shows — WebView, GPU drawing,
// native views, all at once — and is therefore not tied to any single
// view type. It depends on neither the WebView nor the agent debug
// server; it's a plain Graphics API any app can use:
//
//     Graphics::ScreenRecorder recorder;
//     recorder.start(window, "session.mp4");
//     ... app runs ...
//     recorder.stop();
//
// Window::startScreenRecording() wraps this for convenience, and the
// agent debug server drives one of these when a debug-mode build is
// asked to record — but those are just callers.
//
// macOS only today; on other platforms start() returns false with an
// explanatory error. The window must be visible (the window server has
// nothing to capture otherwise), and on recent macOS the process needs
// Screen Recording permission.
class ScreenRecorder
{
public:
    using Options = ScreenRecorderOptions;

    ScreenRecorder();
    ~ScreenRecorder();

    ScreenRecorder(const ScreenRecorder&) = delete;
    ScreenRecorder& operator=(const ScreenRecorder&) = delete;

    // Begin recording `window` (or the window containing `view`) to
    // `path` as MP4. Returns false and sets *error on failure — already
    // recording, no on-screen window, missing permission, or an encoder
    // error. Parent directories of `path` are created.
    bool start(Window& window,
               const std::string& path,
               Options options = {},
               std::string* error = nullptr);
    bool start(View& view,
               const std::string& path,
               Options options = {},
               std::string* error = nullptr);

    // Finish the file and stop capturing. Blocks until the MP4 is
    // flushed to disk. Returns the output path, or an empty string if
    // nothing was recording.
    std::string stop();

    bool isRecording() const;

private:
    bool startWindow(void* nativeWindow,
                     const std::string& path,
                     Options options,
                     std::string* error);

    struct Native;
    Pimpl<Native> impl;
};

// One-shot capture of the *composited* window image — the same window
// server source the recorder uses, so it captures whatever the window
// shows (WebView, GPU, native), not any one view's contents. Returns an
// invalid Image (operator bool == false) and sets *error on failure.
// Window::captureImage() wraps this. macOS only; other platforms fail.
Image captureWindowImage(Window& window, std::string* error = nullptr);
Image captureViewImage(View& view, std::string* error = nullptr);

} // namespace eacp::Graphics
