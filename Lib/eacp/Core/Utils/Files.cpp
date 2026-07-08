#include "Files.h"
#include <fstream>
#include <sstream>

namespace eacp::Files
{
std::string readFile(const std::string& path)
{
    auto stream = std::ifstream(path);

    if (!stream.is_open())
        return {};

    auto buffer = std::ostringstream();
    buffer << stream.rdbuf();
    return buffer.str();
}

std::filesystem::path getTemporaryDirectory()
{
  // A fixed, launch-method-independent location so every process (Finder,
  // `open`, terminal) agrees. /tmp is stable on macOS/Linux; the system
  // temp dir is the equivalent on Windows.
#ifdef _WIN32
    auto dir = std::filesystem::temp_directory_path();
#else
    auto dir = std::filesystem::path {"/tmp"};
#endif
    return dir;
}


std::string filenameFromPath(const std::string& path)
{
    auto separator = path.find_last_of("/\\");

    if (separator != std::string::npos)
        return path.substr(separator + 1);

    return path;
}
} // namespace eacp::Files
