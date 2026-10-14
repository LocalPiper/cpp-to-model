#include "cfg_out.h"

#include <ostream>

#include "../output_utils.h"
#include "../text_utils.h"

namespace cpp2model {

void emitCfgJson(std::ostream &os, const Cfg &cfg) {
  os << "{\"func_decl\":\"" << escapeJson(cfg.func_decl) << "\",\"blocks\":[";
  for (size_t i = 0; i < cfg.blocks.size(); ++i) {
    if (i)
      os << ",";
    const BB &bb = cfg.blocks[i];
    os << "{\"kind\":\"bb\",\"index\":" << i << ",\"statements\":[";
    for (size_t k = 0; k < bb.statements.size(); ++k) {
      if (k)
        os << ",";
      os << "\"" << escapeJson(bb.statements[k]) << "\"";
    }
    os << "],\"term\":";
    emitTerminatorJson(os, bb.term);
    os << "}";
  }
  os << "]}";
}

} // namespace cpp2model