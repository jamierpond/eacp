#pragma once

#include <spawn.h>
#include <string>

namespace eacp::Processes
{
// Adds a chdir to `directory` to the child's file actions. False where this
// system cannot spawn into a directory.
bool addSpawnWorkingDirectory(posix_spawn_file_actions_t& actions,
                              const std::string& directory);
} // namespace eacp::Processes
