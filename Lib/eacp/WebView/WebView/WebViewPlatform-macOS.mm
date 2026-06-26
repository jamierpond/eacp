#import <AppKit/AppKit.h>
#import <WebKit/WebKit.h>

#include "WebViewPlatform-Apple.h"

#include <eacp/Core/ObjC/Strings.h>

@interface EacpDragSource : NSObject <NSDraggingSource>
@end

@implementation EacpDragSource

- (NSDragOperation)draggingSession:(NSDraggingSession*)session
    sourceOperationMaskForDraggingContext:(NSDraggingContext)context
{
    return NSDragOperationCopy;
}

@end

@interface EacpDragWebView : WKWebView
@property(nonatomic) BOOL eacpAcceptFirstMouse;
- (void)armFileDragWithPaths:(const eacp::Vector<std::string>&)paths;
- (void)setFileDragStartedCallback:(eacp::Callback)callback;
- (void)armWindowDrag;
@end

namespace eacp::Graphics::detail
{
namespace
{
EacpDragSource* sharedDragSource()
{
    static EacpDragSource* source = [[EacpDragSource alloc] init];
    return source;
}
} // namespace

bool beginFileDrag(WKWebView* webView,
                   NSEvent* event,
                   const Vector<std::string>& paths)
{
    if (webView == nil || event == nil || paths.empty())
        return false;

    auto* source = sharedDragSource();
    constexpr CGFloat iconSize = 64.0;
    constexpr CGFloat stackOffset = 8.0;

    auto* items = [NSMutableArray arrayWithCapacity:paths.size()];
    auto* workspace = [NSWorkspace sharedWorkspace];
    auto anchor = [webView convertPoint:event.locationInWindow fromView:nil];

    NSUInteger index = 0;
    for (const auto& path: paths)
    {
        auto* nsPath = Strings::toNSString(path);
        auto* fileURL = [NSURL fileURLWithPath:nsPath];

        if (fileURL == nil)
            continue;

        auto* item = [[NSDraggingItem alloc] initWithPasteboardWriter:fileURL];

        auto* icon = [workspace iconForFile:nsPath];
        icon.size = NSMakeSize(iconSize, iconSize);

        auto offset = (CGFloat) index * stackOffset;
        [item setDraggingFrame:NSMakeRect(anchor.x - iconSize / 2 + offset,
                                          anchor.y - iconSize / 2 - offset,
                                          iconSize,
                                          iconSize)
                      contents:icon];

        [items addObject:item];
        [item release];
        ++index;
    }

    if (items.count == 0)
        return false;

    [webView beginDraggingSessionWithItems:items event:event source:source];
    return true;
}

WKWebView* createWebView(WKWebViewConfiguration* config,
                         const WebKitOptions& options)
{
    auto rect = CGRectMake(0, 0, 100, 100);
    auto* webView = [[EacpDragWebView alloc] initWithFrame:rect
                                             configuration:config];
    webView.eacpAcceptFirstMouse = options.acceptFirstMouse;
    return webView;
}

void armFileDrag(WKWebView* webView, const Vector<std::string>& paths)
{
    if (![webView isKindOfClass:[EacpDragWebView class]])
        return;

    [(EacpDragWebView*) webView armFileDragWithPaths:paths];
}

void setFileDragStartedCallback(WKWebView* webView, Callback callback)
{
    if (![webView isKindOfClass:[EacpDragWebView class]])
        return;

    [(EacpDragWebView*) webView setFileDragStartedCallback:std::move(callback)];
}

void armWindowDrag(WKWebView* webView)
{
    if (![webView isKindOfClass:[EacpDragWebView class]])
        return;

    [(EacpDragWebView*) webView armWindowDrag];
}

void performWindowControl(WKWebView* webView, const std::string& action)
{
    NSWindow* window = webView.window;
    if (window == nil)
        return;

    if (action == "minimize")
    {
        [window miniaturize:nil];
        return;
    }

    if (action == "maximize")
    {
        // zoom: is itself a toggle — it restores the saved frame when the
        // window is already zoomed. Report the resulting state back so the
        // page's data-eacp-maximized attribute tracks reality.
        [window zoom:nil];
        auto* script = window.zoomed ? @"window.__eacpSetMaximized(true)"
                                     : @"window.__eacpSetMaximized(false)";
        [webView evaluateJavaScript:script completionHandler:nil];
        return;
    }

    if (action == "close")
    {
        // performClose: respects windowShouldClose:, but beeps and refuses
        // on windows without a close button — exactly the frameless windows
        // that need web-rendered controls — so those close directly.
        // windowWillClose still fires either way, so the window's quit
        // policy runs.
        if ((window.styleMask & NSWindowStyleMaskClosable) != 0)
            [window performClose:nil];
        else
            [window close];
    }
}

void attachWKWebViewToParent(WKWebView* webView, void* parentHandle)
{
    auto* parentView = (__bridge NSView*) parentHandle;
    [parentView addSubview:webView];
}

void applyNativeZoom(WKWebView* webView, double clamped, double&)
{
    webView.pageZoom = clamped;
}

double readNativeZoom(WKWebView* webView, double)
{
    return webView.pageZoom;
}

WebView* findFocusedWebView()
{
    auto* keyWindow = [NSApp keyWindow];

    if (keyWindow == nil)
        return nullptr;

    for (auto* view: registeredWebViews())
    {
        auto* wkWebView = wkWebViewOf(view);

        if (wkWebView != nil && wkWebView.window == keyWindow)
            return view;
    }

    return nullptr;
}
} // namespace eacp::Graphics::detail

@implementation EacpDragWebView
{
    eacp::Vector<std::string> armedPaths;
    eacp::Callback fileDragStartedCallback;
    NSPoint mouseDownLocation;
    BOOL dragArmed;
    BOOL dragStarted;
    BOOL windowDragArmed;
}

- (void)armFileDragWithPaths:(const eacp::Vector<std::string>&)paths
{
    armedPaths = paths;
    dragArmed = ! paths.empty();
}

- (void)setFileDragStartedCallback:(eacp::Callback)callback
{
    fileDragStartedCallback = std::move(callback);
}

- (void)armWindowDrag
{
    windowDragArmed = YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)event
{
    if (self.eacpAcceptFirstMouse)
        return YES;
    return [super acceptsFirstMouse:event];
}

- (void)mouseDown:(NSEvent*)event
{
    mouseDownLocation = event.locationInWindow;
    dragArmed = NO;
    dragStarted = NO;
    windowDragArmed = NO;
    armedPaths.clear();
    [super mouseDown:event];
}

- (void)mouseDragged:(NSEvent*)event
{
    if (dragArmed && ! dragStarted)
    {
        auto dx = event.locationInWindow.x - mouseDownLocation.x;
        auto dy = event.locationInWindow.y - mouseDownLocation.y;
        constexpr CGFloat threshold = 4.0;

        if (dx * dx + dy * dy >= threshold * threshold)
        {
            dragStarted = YES;
            dragArmed = NO;
            auto didStart =
                eacp::Graphics::detail::beginFileDrag(self, event, armedPaths);
            armedPaths.clear();
            if (didStart && fileDragStartedCallback)
            {
                auto callback = fileDragStartedCallback;
                dispatch_async(dispatch_get_main_queue(),
                               ^{ callback(); });
            }
            return;
        }
    }

    if (windowDragArmed && ! dragStarted)
    {
        dragStarted = YES;
        windowDragArmed = NO;
        [self.window performWindowDragWithEvent:event];
        return;
    }

    [super mouseDragged:event];
}

- (void)mouseUp:(NSEvent*)event
{
    dragArmed = NO;
    dragStarted = NO;
    windowDragArmed = NO;
    armedPaths.clear();
    [super mouseUp:event];
}

@end
