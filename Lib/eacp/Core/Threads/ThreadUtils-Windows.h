#pragma once

#include <eacp/Core/Utils/WinInclude.h>

#include "ThreadUtils.h"
#include <winrt/Windows.System.h>

namespace eacp::Threads
{
namespace System = winrt::Windows::System;

void initMainThread();

System::DispatcherQueue getDispatcherQueue();
System::DispatcherQueueController getDispatcherQueueController();
}
