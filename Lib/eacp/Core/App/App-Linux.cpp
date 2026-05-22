#include "App.h"

#include <cassert>

namespace eacp::Apps
{
// TODO: wire to xdg-open via fork/exec (avoid system() — shell metachars
// in URLs are a hazard).
void openExternalURL(const std::string&)
{
    assert(false && "openExternalURL not implemented on Linux");
}
} // namespace eacp::Apps
