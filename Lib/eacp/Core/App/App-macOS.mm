#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include "App.h"

namespace eacp::Apps
{
void openExternalURL(const std::string& url)
{
    auto* nsString = [NSString stringWithUTF8String:url.c_str()];

    if (nsString == nil)
        return;

    auto* nsUrl = [NSURL URLWithString:nsString];

    if (nsUrl == nil)
        return;

    [[NSWorkspace sharedWorkspace] openURL:nsUrl];
}

std::optional<std::string> chooseDirectory()
{
    auto* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = NO;
    panel.resolvesAliases = YES;

    if ([panel runModal] != NSModalResponseOK)
        return std::nullopt;

    auto* url = panel.URLs.firstObject;

    if (url == nil)
        return std::nullopt;

    return std::string(url.fileSystemRepresentation);
}
} // namespace eacp::Apps
