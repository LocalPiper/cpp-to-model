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
  os << "{\"kind\":\"" << termKindName(t.Kind) << "\"";
  switch (t.Kind) {
  case TermKind::Fallthrough:
    break;
  case TermKind::Jump:
    os << ",\"target\":" << t.Target;
    break;
  case TermKind::Branch:
    os << ",\"cond\":\"" << escapeJson(t.Cond) << "\""
       << ",\"on_true\":" << t.OnTrue << ",\"on_false\":" << t.OnFalse;
    break;
  case TermKind::Switch:
    os << ",\"switch_val\":\"" << escapeJson(t.SwitchVal) << "\""
       << ",\"cases\":[";
    for (size_t i = 0; i < t.Cases.size(); ++i) {
      if (i)
        os << ",";
      os << "{\"value\":" << t.Cases[i].first
         << ",\"target\":" << t.Cases[i].second << "}";
    }
    os << "],\"default_target\":" << t.DefaultTarget;
    break;
  case TermKind::Return:
    os << ",\"ret_val\":\"" << escapeJson(t.RetVal) << "\"";
    break;
  }
  os << "}";
}

} // namespace cpp2model