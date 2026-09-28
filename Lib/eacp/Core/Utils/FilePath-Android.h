#pragma once

#include <string>

namespace eacp
{
// The app's private storage (ANativeActivity::internalDataPath), which every
// FilePath directory lives under on Android. Set once, as the activity starts.
void setAndroidDataDirectory(std::string path);
} // namespace eacp
