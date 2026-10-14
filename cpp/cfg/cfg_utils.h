#pragma once
#include <string>
#include <vector>

namespace cpp2model {

std::string stringifyFuncDecl(const std::string &func_name,
                              const std::vector<std::string> &func_params);

} // namespace cpp2model