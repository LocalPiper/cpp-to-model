#pragma once

#include <cstddef>
#include <string>

namespace cpp2model {

std::string escapeJson(const std::string &s);

std::string displayEscape(const std::string &s);

std::string xmlEscape(const std::string &s);

std::string truncate(const std::string &s, size_t n);

} // namespace cpp2model