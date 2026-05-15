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

std::string filenameFromPath(const std::string& path)
{
    auto separator = path.find_last_of("/\\");

    if (separator != std::string::npos)
        return path.substr(separator + 1);

    return path;
}
} // namespace eacp::Files
