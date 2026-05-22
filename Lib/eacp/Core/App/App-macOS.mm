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
} // namespace eacp::Apps
