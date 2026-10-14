#include "text_utils.h"

#include <cstdio>
#include <string>

namespace cpp2model {

std::string escapeJson(const std::string &s) {
  std::string r;
  for (unsigned char c : s) {
    switch (c) {
    case '"':
      r += "\\\"";
      break;
    case '\\':
      r += "\\\\";
      break;
    case '\n':
      r += "\\n";
      break;
    case '\r':
      r += "\\r";
      break;
    case '\t':
      r += "\\t";
      break;
    default:
      if (c < 0x20) {
        char buf[8];
        std::snprintf(buf, sizeof buf, "\\u%04x", c);
        r += buf;
      } else {
        r += static_cast<char>(c);
      }
    }
  }
  return r;
}

std::string displayEscape(const std::string &s) {
  std::string r;
  for (unsigned char c : s) {
    switch (c) {
    case '\n':
      r += "\\n";
      break;
    case '\r':
      r += "\\r";
      break;
    case '\t':
      r += "\\t";
      break;
    default:
      if (c < 0x20)
        r += '.';
      else
        r += static_cast<char>(c);
    }
  }
  return r;
}

std::string xmlEscape(const std::string &s) {
  std::string r;
  for (char c : s) {
    switch (c) {
    case '&':
      r += "&amp;";
      break;
    case '<':
      r += "&lt;";
      break;
    case '>':
      r += "&gt;";
      break;
    case '"':
      r += "&quot;";
      break;
    case '\'':
      r += "&apos;";
      break;
    default:
      r += c;
    }
  }
  return r;
}

std::string truncate(const std::string &s, size_t n) {
  if (s.size() <= n)
    return s;
  return s.substr(0, n - 3) + "...";
}

} // namespace cpp2model