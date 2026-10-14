#pragma once
#include <string>

namespace cpp2model {

std::string dot_to_svg(const std::string &dot_src, std::string &err);

} // namespace cpp2model