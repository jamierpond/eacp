#pragma once

#include "VulkanContext.h"

#include <eacp/Graphics/View/View-Linux.h>

// The window-system half of presenting, one file per platform: each build
// defines only its own VK_USE_PLATFORM_* macros, so only its own create-info
// structs exist.

namespace eacp::GPU
{
// The instance extensions that make surfaces for this build's window systems.
Vector<const char*> windowSystemSurfaceExtensions();

// False, and `surface` untouched, when the handle's window system is not this
// build's or the driver did not offer its extension.
bool createWindowSystemSurface(VkInstance instance,
                               const Graphics::NativeSurfaceHandle& handle,
                               VkSurfaceKHR& surface);
} // namespace eacp::GPU
