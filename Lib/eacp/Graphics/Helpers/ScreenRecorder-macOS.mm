#include "ScreenRecorder.h"

#include "../View/View.h"
#include "../Window/Window.h"

#include <eacp/Core/Threads/Timer.h>

#include <dlfcn.h>
#include <filesystem>
#include <memory>

#import <AVFoundation/AVFoundation.h>
#import <Cocoa/Cocoa.h>
#import <CoreMedia/CoreMedia.h>

namespace eacp::Graphics
{

namespace
{
// CGWindowListCreateImage is obsoleted in the macOS 15 SDK (ScreenCapture
// Kit is the supported replacement and the eventual migration target),
// but the symbol is still present at runtime, so resolve it dynamically
// to capture the composited window image without an SDK dependency on a
// removed declaration.
using CreateWindowImageFn = CGImageRef (*) (CGRect,
                                            CGWindowListOption,
                                            CGWindowID,
                                            CGWindowImageOption);

CGImageRef captureWindowImage(CGWindowID windowID)
{
    static auto* fn = reinterpret_cast<CreateWindowImageFn>(
        dlsym(RTLD_DEFAULT, "CGWindowListCreateImage"));
    if (!fn)
        return nullptr;

    return fn(CGRectNull,
              kCGWindowListOptionIncludingWindow,
              windowID,
              kCGWindowImageBoundsIgnoreFraming | kCGWindowImageBestResolution);
}
} // namespace

struct ScreenRecorder::Native
{
    CGWindowID windowID = kCGNullWindowID;
    int fps = 30;
    int width = 0;
    int height = 0;
    long frameIndex = 0;
    bool recording = false;
    std::string path;

    AVAssetWriter* writer = nil;
    AVAssetWriterInput* input = nil;
    AVAssetWriterInputPixelBufferAdaptor* adaptor = nil;
    CVPixelBufferRef pixelBuffer = nullptr;

    std::unique_ptr<Threads::Timer> timer;

    ~Native() { releasePixelBuffer(); }

    void releasePixelBuffer()
    {
        if (pixelBuffer)
        {
            CVPixelBufferRelease(pixelBuffer);
            pixelBuffer = nullptr;
        }
    }

    // Captures the live composited window image. Returns nil when the
    // window has nothing on screen or the process lacks permission.
    CGImageRef copyWindowImage() const { return captureWindowImage(windowID); }

    void appendImage(CGImageRef image)
    {
        if (!input.isReadyForMoreMediaData)
            return;

        CVPixelBufferLockBaseAddress(pixelBuffer, 0);

        auto* base = CVPixelBufferGetBaseAddress(pixelBuffer);
        auto bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);
        auto colorSpace = CGColorSpaceCreateDeviceRGB();
        auto context = CGBitmapContextCreate(
            base,
            static_cast<std::size_t>(width),
            static_cast<std::size_t>(height),
            8,
            bytesPerRow,
            colorSpace,
            kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);

        // Drawing into the pixel-buffer-backed context lands the frame
        // the right way up: the context's bottom-left origin and
        // CGContextDrawImage's flip cancel out. Scales if the window has
        // since resized.
        CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);

        CGContextRelease(context);
        CGColorSpaceRelease(colorSpace);
        CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

        [adaptor appendPixelBuffer:pixelBuffer
              withPresentationTime:CMTimeMake(frameIndex, fps)];
        ++frameIndex;
    }

    void captureFrame()
    {
        if (!recording)
            return;

        if (auto image = copyWindowImage())
        {
            appendImage(image);
            CGImageRelease(image);
        }
    }
};

ScreenRecorder::ScreenRecorder() = default;
ScreenRecorder::~ScreenRecorder()
{
    stop();
}

bool ScreenRecorder::isRecording() const
{
    return impl->recording;
}

bool ScreenRecorder::start(Window& window,
                           const std::string& path,
                           Options options,
                           std::string* error)
{
    return startWindow(window.getHandle(), path, options, error);
}

bool ScreenRecorder::start(View& view,
                           const std::string& path,
                           Options options,
                           std::string* error)
{
    auto* nsView = (__bridge NSView*) view.getHandle();
    return startWindow(nsView ? (__bridge void*) nsView.window : nullptr,
                       path,
                       options,
                       error);
}

namespace
{
[[nodiscard]] bool fail(std::string* error, const std::string& message)
{
    if (error)
        *error = message;
    return false;
}
} // namespace

bool ScreenRecorder::startWindow(void* nativeWindow,
                                 const std::string& path,
                                 Options options,
                                 std::string* error)
{
    if (impl->recording)
        return fail(error, "ScreenRecorder: already recording");

    auto* window = (__bridge NSWindow*) nativeWindow;
    if (!window)
        return fail(error, "ScreenRecorder: no native window to record");

    impl->windowID = static_cast<CGWindowID>(window.windowNumber);
    impl->fps = options.frameRateHz > 0 ? options.frameRateHz : 30;
    impl->frameIndex = 0;
    impl->path = path;

    // The first capture both proves we can see the window (permission +
    // visibility) and fixes the output dimensions.
    auto firstImage = impl->copyWindowImage();
    if (!firstImage)
        return fail(error,
                    "ScreenRecorder: could not capture the window — it must "
                    "be visible and the process needs Screen Recording "
                    "permission");

    impl->width = static_cast<int>(CGImageGetWidth(firstImage));
    impl->height = static_cast<int>(CGImageGetHeight(firstImage));

    auto fsPath = std::filesystem::path {path};
    if (fsPath.has_parent_path())
        std::filesystem::create_directories(fsPath.parent_path());
    std::filesystem::remove(fsPath);

    auto* url = [NSURL fileURLWithPath:@(path.c_str())];
    NSError* writerError = nil;
    impl->writer = [AVAssetWriter assetWriterWithURL:url
                                            fileType:AVFileTypeMPEG4
                                               error:&writerError];
    if (!impl->writer)
    {
        CGImageRelease(firstImage);
        return fail(error,
                    std::string {"ScreenRecorder: "}
                        + writerError.localizedDescription.UTF8String);
    }

    NSDictionary* videoSettings = @{
        AVVideoCodecKey : AVVideoCodecTypeH264,
        AVVideoWidthKey : @(impl->width),
        AVVideoHeightKey : @(impl->height),
    };
    impl->input =
        [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo
                                           outputSettings:videoSettings];
    impl->input.expectsMediaDataInRealTime = YES;

    NSDictionary* bufferAttributes = @{
        (id) kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA),
        (id) kCVPixelBufferWidthKey : @(impl->width),
        (id) kCVPixelBufferHeightKey : @(impl->height),
    };
    impl->adaptor = [AVAssetWriterInputPixelBufferAdaptor
        assetWriterInputPixelBufferAdaptorWithAssetWriterInput:impl->input
                                   sourcePixelBufferAttributes:bufferAttributes];

    [impl->writer addInput:impl->input];
    if (![impl->writer startWriting])
    {
        CGImageRelease(firstImage);
        impl->writer = nil;
        impl->input = nil;
        impl->adaptor = nil;
        return fail(error, "ScreenRecorder: failed to start writing");
    }
    [impl->writer startSessionAtSourceTime:kCMTimeZero];

    CVPixelBufferCreate(kCFAllocatorDefault,
                        static_cast<std::size_t>(impl->width),
                        static_cast<std::size_t>(impl->height),
                        kCVPixelFormatType_32BGRA,
                        nullptr,
                        &impl->pixelBuffer);

    impl->recording = true;
    impl->appendImage(firstImage);
    CGImageRelease(firstImage);

    auto* native = impl.get();
    impl->timer =
        std::make_unique<Threads::Timer>([native] { native->captureFrame(); },
                                         impl->fps);

    return true;
}

std::string ScreenRecorder::stop()
{
    if (!impl->recording)
        return {};

    impl->recording = false;
    impl->timer.reset();

    [impl->input markAsFinished];

    auto semaphore = dispatch_semaphore_create(0);
    [impl->writer finishWritingWithCompletionHandler:^{
        dispatch_semaphore_signal(semaphore);
    }];
    dispatch_semaphore_wait(
        semaphore,
        dispatch_time(DISPATCH_TIME_NOW, (int64_t) (10 * NSEC_PER_SEC)));

    auto path = impl->path;
    impl->releasePixelBuffer();
    impl->writer = nil;
    impl->input = nil;
    impl->adaptor = nil;

    return path;
}

} // namespace eacp::Graphics
