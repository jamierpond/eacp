#include "Strings.h"

#include <cctype>

namespace eacp::Strings
{
std::string trim(const std::string& s)
{
    auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

std::string toLower(std::string s)
{
    for (auto& c: s)
        c = (char) std::tolower((unsigned char) c);
    return s;
}
}
