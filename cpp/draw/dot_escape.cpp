#include "dot_escape.h"

namespace cpp2model {

std::string dotEscape(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      break;
    case '\t':
      out += "    ";
      break;
    default:
      out += c;
      break;
    }
  }
  return out;
}

} // namespace cpp2model