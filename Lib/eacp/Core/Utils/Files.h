#pragma once

#include <string>
#include <filesystem>

namespace eacp::Files
{
std::string readFile(const std::string& path);
std::string getBundleResourcePath(const std::string& filename);
std::string filenameFromPath(const std::string& path);
std::filesystem::path getTemporaryDirectory();
} // namespace eacp::Files
