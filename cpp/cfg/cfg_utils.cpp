#include "cfg_utils.h"

namespace cpp2model {

std::string stringifyFuncDecl(const std::string &funcName,
                              const std::vector<std::string> &params) {
  std::string res = funcName + "(";
  bool first = true;
  for (const std::string &p : params) {
    if (p.empty())
      continue;
    if (!first)
      res += ",";
    res += p;
    first = false;
  }
  res += ")";
  return res;
}

} // namespace cpp2model