#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

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

    [[UIApplication sharedApplication] openURL:nsUrl
                                       options:@{}
                             completionHandler:nil];
}
} // namespace eacp::Apps
