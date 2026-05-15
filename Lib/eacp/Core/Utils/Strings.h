#pragma once

#include <string>

namespace eacp::Strings
{
std::string trim(const std::string& s);
std::string toLower(std::string s);

bool equalsCaseInsensitive(const std::string& a, const std::string& b);
} // namespace eacp::Strings
