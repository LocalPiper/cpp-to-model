#pragma once

#include <string>

namespace cpp2model {

std::string escape_json(const std::string& s);

std::string display_escape(const std::string& s);

std::string xml_escape(const std::string& s);

std::string truncate(const std::string& s, size_t n);

}  // namespace cpp2model