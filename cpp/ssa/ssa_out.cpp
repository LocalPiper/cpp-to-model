#include "ssa_out.h"

#include <cassert>
#include <ostream>

#include "../output_utils.h"
#include "../text_utils.h"

namespace cpp2model {

namespace {

void emitTac(std::ostream &os, const TacOp &op) {
  os << "\"" << escapeJson(op.dst) << " = ";
  if (op.op == "=") {
    assert(op.args.size() == 1);
    os << escapeJson(op.args[0]);
  } else if (op.args.size() == 1) {
    const bool spaced = (op.op == "sizeof" || op.op == "alignof");
    os << escapeJson(op.op) << (spaced ? " " : "") << escapeJson(op.args[0]);
  } else if (op.args.size() == 2) {
    os << escapeJson(op.args[0]) << " " << escapeJson(op.op) << " "
       << escapeJson(op.args[1]);
  } else {
    assert(false && "TacOp has unsupported arity");
  }
  os << "\"";
}

void emitPhi(std::ostream &os, const PhiNode &phi) {
  os << "{\"var\":\"" << escapeJson(phi.var) << "\""
     << ",\"base\":\"" << escapeJson(phi.base) << "\""
     << ",\"incoming\":[";
  for (size_t i = 0; i < phi.incoming.size(); ++i) {
    if (i)
      os << ",";
    os << "{\"value\":\"" << escapeJson(phi.incoming[i].value) << "\""
       << ",\"pred\":" << phi.incoming[i].pred_block << "}";
  }
  os << "]}";
}

} // namespace

void emitSsaJson(std::ostream &os, const SsaCfg &cfg) {
  os << "{\"func_decl\":\"" << escapeJson(cfg.func_decl) << "\",\"blocks\":[";
  for (size_t i = 0; i < cfg.blocks.size(); ++i) {
    if (i)
      os << ",";
    const SsaBB &bb = cfg.blocks[i];

    os << "{\"kind\":\"bb\",\"index\":" << i;

    if (i < cfg.rpo_num.size())
      os << ",\"rpo_num\":" << cfg.rpo_num[i];
    if (i < cfg.idom.size())
      os << ",\"idom\":" << cfg.idom[i];

    os << ",\"df\":[";
    if (i < cfg.df.size()) {
      for (size_t k = 0; k < cfg.df[i].size(); ++k) {
        if (k)
          os << ",";
        os << cfg.df[i][k];
      }
    }
    os << "]";

    os << ",\"preds\":[";
    for (size_t k = 0; k < bb.preds.size(); ++k) {
      if (k)
        os << ",";
      os << bb.preds[k];
    }
    os << "],\"succs\":[";
    for (size_t k = 0; k < bb.succs.size(); ++k) {
      if (k)
        os << ",";
      os << bb.succs[k];
    }
    os << "]";

    os << ",\"phis\":[";
    for (size_t k = 0; k < bb.phis.size(); ++k) {
      if (k)
        os << ",";
      emitPhi(os, bb.phis[k]);
    }
    os << "]";

    os << ",\"statements\":[";
    for (size_t k = 0; k < bb.ops.size(); ++k) {
      if (k)
        os << ",";
      emitTac(os, bb.ops[k]);
    }
    os << "]";

    os << ",\"term\":";
    emitTerminatorJson(os, bb.term);
    os << "}";
  }
  os << "]}";
}

} // namespace cpp2model