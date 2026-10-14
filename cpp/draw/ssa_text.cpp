#include "ssa_text.h"

namespace cpp2model {

std::string ssaTacText(const TacOp &op) {
  std::string out = op.dst + " = ";
  if (op.op == "=") {
    out += op.args.empty() ? "" : op.args[0];
  } else if (op.args.size() == 1) {
    const bool spaced = (op.op == "sizeof" || op.op == "alignof");
    out += op.op;
    if (spaced)
      out += " ";
    out += op.args[0];
  } else if (op.args.size() == 2) {
    out += op.args[0] + " " + op.op + " " + op.args[1];
  } else {
    out += op.op;
  }
  return out;
}

std::string ssaPhiText(const PhiNode &p) {
  std::string out = p.var + " = φ(";
  for (size_t i = 0; i < p.incoming.size(); ++i) {
    if (i)
      out += ", ";
    out += p.incoming[i].value;
    out += " [BB" + std::to_string(p.incoming[i].pred_block) + "]";
  }
  out += ")";
  return out;
}

} // namespace cpp2model