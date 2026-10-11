#include "cfg_out.h"

#include <ostream>
#include <variant>

#include "../text_utils.h"

namespace cpp2model {

void emit_cfg_json(std::ostream& os, const Cfg& cfg) {
    os << "{\"func_decl\":\"" << escape_json(cfg.func_decl) << "\",\"blocks\":[";
    for (size_t i = 0; i < cfg.blocks.size(); ++i) {
        if (i) os << ",";
        if (std::holds_alternative<BB>(cfg.blocks[i])) {
            const BB& bb = std::get<BB>(cfg.blocks[i]);
            os << "{\"kind\":\"bb\",\"statements\":[";
            for (size_t k = 0; k < bb.statements.size(); ++k) {
                if (k) os << ",";
                os << "\"" << escape_json(bb.statements[k]) << "\"";
            }
            os << "]}";
        } else {
            const Condition& c = std::get<Condition>(cfg.blocks[i]);
            os << "{\"kind\":\"condition\",\"cond\":\"" << escape_json(c.cond)
               << "\",\"on_true\":" << c.on_true << ",\"on_false\":" << c.on_false << "}";
        }
    }
    os << "]}";
}

}  // namespace cpp2model
