#include "ScreenRecorder.h"

#include "../View/View.h"
#include "../Window/Window.h"

#include <cmath>
#include <filesystem>
#include <string>

#import <AVFoundation/AVFoundation.h>
#import <Cocoa/Cocoa.h>
#import <CoreMedia/CoreMedia.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

@class EacpRecorderOutput;

namespace eacp::Graphics
{

// Captures the window through ScreenCaptureKit (the supported window
// capture API on modern macOS) and hands each delivered frame's pixel
// buffer straight to AVAssetWriter — no intermediate image copy and no
// reliance on the obsoleted CGWindowList APIs.
struct ScreenRecorder::Native
{
    SCStream* stream = nil;
    EacpRecorderOutput* output = nil;
    dispatch_queue_t queue = nil;

    AVAssetWriter* writer = nil;
    AVAssetWriterInput* input = nil;
    AVAssetWriterInputPixelBufferAdaptor* adaptor = nil;

    bool recording = false;
    bool sessionStarted = false;
    int fps = 30;
    std::string path;

    // Called on the capture queue for every delivered frame.
    void handleSampleBuffer(CMSampleBufferRef sampleBuffer)
    {
        if (!recording || !CMSampleBufferDataIsReady(sampleBuffer))
            return;

        auto* attachments = (__bridge NSArray*) CMSampleBufferGetSampleAttachmentsArray(
            sampleBuffer, false);
        if (attachments.count == 0)
            return;

        // Only "complete" frames carry fresh pixels; idle / blank frames
        // (sent when nothing changed) would just bloat the file.
        auto status = static_cast<SCFrameStatus>(
            [attachments[0][SCStreamFrameInfoStatus] integerValue]);
        if (status != SCFrameStatusComplete)
            return;

        auto pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
        if (!pixelBuffer)
            return;

        auto pts = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);

        // Start the movie's timeline at the first real frame, so the
        // video plays back in wall-clock time.
        if (!sessionStarted)
        {
            [writer startSessionAtSourceTime:pts];
            sessionStarted = true;
        }

        if (input.isReadyForMoreMediaData)
            [adaptor appendPixelBuffer:pixelBuffer withPresentationTime:pts];
    }
};

} // namespace eacp::Graphics

@interface EacpRecorderOutput : NSObject <SCStreamOutput, SCStreamDelegate>
// Set by the recorder; ScreenRecorder::Native is private, so frames are
// forwarded through a block rather than a typed back-pointer.
@property (nonatomic, copy) void (^onScreenSample)(CMSampleBufferRef);
@end

@implementation EacpRecorderOutput
- (void)stream:(SCStream*)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type
{
    if (type == SCStreamOutputTypeScreen && self.onScreenSample)
        self.onScreenSample(sampleBuffer);
}

- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error
{
}
@end

namespace eacp::Graphics
{

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

// Blocks the caller until an async ScreenCaptureKit handler signals, or
// the timeout elapses. SCKit completion handlers run on background
// queues, so this never deadlocks the main thread.
bool waitFor(dispatch_semaphore_t semaphore, double seconds)
{
    auto deadline =
        dispatch_time(DISPATCH_TIME_NOW, (int64_t) (seconds * NSEC_PER_SEC));
    return dispatch_semaphore_wait(semaphore, deadline) == 0;
}
} // namespace

bool ScreenRecorder::startWindow(void* nativeWindow,
                                 const std::string& path,
                                 Options options,
                                 std::string* error)
{
    if (impl->recording)
        return fail(error, "ScreenRecorder: already recording");

    auto* nsWindow = (__bridge NSWindow*) nativeWindow;
    if (!nsWindow)
        return fail(error, "ScreenRecorder: no native window to record");

    if (@available(macOS 12.3, *))
    {
        auto windowID = static_cast<CGWindowID>(nsWindow.windowNumber);
        impl->fps = options.frameRateHz > 0 ? options.frameRateHz : 30;
        impl->path = path;

        // Find the on-screen window ScreenCaptureKit knows about. Empty
        // content / a missing window means it isn't visible or the
        // process lacks Screen Recording permission.
        __block SCWindow* target = nil;
        __block NSError* contentError = nil;
        auto contentReady = dispatch_semaphore_create(0);
        [SCShareableContent
            getShareableContentWithCompletionHandler:^(SCShareableContent* content,
                                                       NSError* err) {
                if (err)
                    contentError = err;
                else
                    for (SCWindow* candidate in content.windows)
                        if (candidate.windowID == windowID)
                        {
                            target = candidate;
                            break;
                        }
                dispatch_semaphore_signal(contentReady);
            }];
        waitFor(contentReady, 5.0);

        if (contentError)
            return fail(error,
                        std::string {"ScreenRecorder: "}
                            + contentError.localizedDescription.UTF8String);
        if (!target)
            return fail(error,
                        "ScreenRecorder: could not find the window — it must "
                        "be visible and the process needs Screen Recording "
                        "permission");

        auto scale = nsWindow.backingScaleFactor;
        auto width = static_cast<int>(std::llround(target.frame.size.width * scale));
        auto height = static_cast<int>(std::llround(target.frame.size.height * scale));
        width -= width & 1; // H.264 wants even dimensions
        height -= height & 1;
        if (width <= 0 || height <= 0)
            return fail(error, "ScreenRecorder: window has no size");

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
            return fail(error,
                        std::string {"ScreenRecorder: "}
                            + writerError.localizedDescription.UTF8String);

        impl->input = [AVAssetWriterInput
            assetWriterInputWithMediaType:AVMediaTypeVideo
                           outputSettings:@{
                               AVVideoCodecKey : AVVideoCodecTypeH264,
                               AVVideoWidthKey : @(width),
                               AVVideoHeightKey : @(height),
                           }];
        impl->input.expectsMediaDataInRealTime = YES;
        impl->adaptor = [AVAssetWriterInputPixelBufferAdaptor
            assetWriterInputPixelBufferAdaptorWithAssetWriterInput:impl->input
                                       sourcePixelBufferAttributes:@{
                                           (id) kCVPixelBufferPixelFormatTypeKey :
                                               @(kCVPixelFormatType_32BGRA),
                                       }];
        [impl->writer addInput:impl->input];
        if (![impl->writer startWriting])
        {
            impl->writer = nil;
            impl->input = nil;
            impl->adaptor = nil;
            return fail(error, "ScreenRecorder: failed to start writing");
        }

        auto* config = [[SCStreamConfiguration alloc] init];
        config.width = static_cast<size_t>(width);
        config.height = static_cast<size_t>(height);
        config.pixelFormat = kCVPixelFormatType_32BGRA;
        config.minimumFrameInterval = CMTimeMake(1, impl->fps);
        config.queueDepth = 6;
        config.showsCursor = NO;

        auto* filter = [[SCContentFilter alloc] initWithDesktopIndependentWindow:target];

        impl->output = [[EacpRecorderOutput alloc] init];
        auto* native = impl.get();
        impl->output.onScreenSample = ^(CMSampleBufferRef sampleBuffer) {
            native->handleSampleBuffer(sampleBuffer);
        };
        impl->queue =
            dispatch_queue_create("ai.eacp.screenrecorder", DISPATCH_QUEUE_SERIAL);
        impl->stream = [[SCStream alloc] initWithFilter:filter
                                          configuration:config
                                               delegate:impl->output];

        NSError* addError = nil;
        [impl->stream addStreamOutput:impl->output
                                 type:SCStreamOutputTypeScreen
                   sampleHandlerQueue:impl->queue
                                error:&addError];
        if (addError)
        {
            impl->stream = nil;
            impl->output = nil;
            impl->writer = nil;
            impl->input = nil;
            impl->adaptor = nil;
            return fail(error,
                        std::string {"ScreenRecorder: "}
                            + addError.localizedDescription.UTF8String);
        }

        impl->recording = true;

        __block NSError* startError = nil;
        auto started = dispatch_semaphore_create(0);
        [impl->stream startCaptureWithCompletionHandler:^(NSError* err) {
            startError = err;
            dispatch_semaphore_signal(started);
        }];
        waitFor(started, 5.0);

        if (startError)
        {
            impl->recording = false;
            impl->stream = nil;
            impl->output = nil;
            impl->writer = nil;
            impl->input = nil;
            impl->adaptor = nil;
            return fail(error,
                        std::string {"ScreenRecorder: "}
                            + startError.localizedDescription.UTF8String);
        }

        return true;
    }

    return fail(error, "ScreenRecorder: screen recording requires macOS 12.3+");
}

std::string ScreenRecorder::stop()
{
    if (!impl->recording)
        return {};

    impl->recording = false;

    if (@available(macOS 12.3, *))
    {
        auto stopped = dispatch_semaphore_create(0);
        [impl->stream stopCaptureWithCompletionHandler:^(NSError*) {
            dispatch_semaphore_signal(stopped);
        }];
        waitFor(stopped, 5.0);
    }

    [impl->input markAsFinished];

    auto finished = dispatch_semaphore_create(0);
    [impl->writer finishWritingWithCompletionHandler:^{
        dispatch_semaphore_signal(finished);
    }];
    waitFor(finished, 10.0);

    auto path = impl->path;
    impl->stream = nil;
    impl->output = nil;
    impl->queue = nil;
    impl->writer = nil;
    impl->input = nil;
    impl->adaptor = nil;
    impl->sessionStarted = false;

    return path;
}

} // namespace eacp::Graphics
