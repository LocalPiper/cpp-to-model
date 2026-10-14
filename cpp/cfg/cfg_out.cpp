#include "cfg_out.h"

#include <ostream>

#include "../output_utils.h"
#include "../text_utils.h"

namespace cpp2model {

void emitCfgJson(std::ostream &os, const Cfg &cfg) {
  os << "{\"FuncDecl\":\"" << escapeJson(cfg.FuncDecl) << "\",\"Blocks\":[";
  for (size_t i = 0; i < cfg.Blocks.size(); ++i) {
    if (i)
      os << ",";
    const BB &bb = cfg.Blocks[i];
    os << "{\"kind\":\"bb\",\"index\":" << i << ",\"Statements\":[";
    for (size_t k = 0; k < bb.Statements.size(); ++k) {
      if (k)
        os << ",";
      os << "\"" << escapeJson(bb.Statements[k]) << "\"";
    }
    os << "],\"Term\":";
    emitTerminatorJson(os, bb.Term);
    os << "}";
  }
  os << "]}";
}

} // namespace cpp2model