#include "output_utils.h"

#include <ostream>

#include "text_utils.h"

namespace cpp2model {

const char *termKindName(TermKind k) {
  switch (k) {
  case TermKind::Fallthrough:
    return "fallthrough";
  case TermKind::Jump:
    return "jump";
  case TermKind::Branch:
    return "branch";
  case TermKind::Switch:
    return "switch";
  case TermKind::Return:
    return "return";
  }
  return "unknown";
}

void emitTerminatorJson(std::ostream &os, const Terminator &t) {
  os << "{\"kind\":\"" << termKindName(t.kind) << "\"";
  switch (t.kind) {
  case TermKind::Fallthrough:
    break;
  case TermKind::Jump:
    os << ",\"target\":" << t.target;
    break;
  case TermKind::Branch:
    os << ",\"cond\":\"" << escapeJson(t.cond) << "\""
       << ",\"on_true\":" << t.on_true << ",\"on_false\":" << t.on_false;
    break;
  case TermKind::Switch:
    os << ",\"switch_val\":\"" << escapeJson(t.switch_val) << "\""
       << ",\"cases\":[";
    for (size_t i = 0; i < t.cases.size(); ++i) {
      if (i)
        os << ",";
      os << "{\"value\":" << t.cases[i].first
         << ",\"target\":" << t.cases[i].second << "}";
    }
    os << "],\"default_target\":" << t.default_target;
    break;
  case TermKind::Return:
    os << ",\"ret_val\":\"" << escapeJson(t.ret_val) << "\"";
    break;
  }
  os << "}";
}

} // namespace cpp2model