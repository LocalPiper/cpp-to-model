#include "cfg_out.h"

#include <ostream>

#include "../text_utils.h"

namespace cpp2model {

namespace {

const char* term_kind_name(TermKind k) {
    switch (k) {
        case TermKind::Fallthrough: return "fallthrough";
        case TermKind::Jump:        return "jump";
        case TermKind::Branch:      return "branch";
        case TermKind::Switch:      return "switch";
        case TermKind::Return:      return "return";
    }
    return "unknown";
}

void emit_terminator(std::ostream& os, const Terminator& t) {
    os << "{\"kind\":\"" << term_kind_name(t.kind) << "\"";
    switch (t.kind) {
        case TermKind::Fallthrough:
            break;
        case TermKind::Jump:
            os << ",\"target\":" << t.target;
            break;
        case TermKind::Branch:
            os << ",\"cond\":\"" << escape_json(t.cond) << "\""
               << ",\"on_true\":"  << t.on_true
               << ",\"on_false\":" << t.on_false;
            break;
        case TermKind::Switch:
            os << ",\"switch_val\":\"" << escape_json(t.switch_val) << "\""
               << ",\"cases\":[";
            for (size_t i = 0; i < t.cases.size(); ++i) {
                if (i) os << ",";
                os << "{\"value\":" << t.cases[i].first
                   << ",\"target\":" << t.cases[i].second << "}";
            }
            os << "],\"default_target\":" << t.default_target;
            break;
        case TermKind::Return:
            os << ",\"ret_val\":\"" << escape_json(t.ret_val) << "\"";
            break;
    }
    os << "}";
}

}  // namespace

void emit_cfg_json(std::ostream& os, const Cfg& cfg) {
    os << "{\"func_decl\":\"" << escape_json(cfg.func_decl) << "\",\"blocks\":[";
    for (size_t i = 0; i < cfg.blocks.size(); ++i) {
        if (i) os << ",";
        const BB& bb = cfg.blocks[i];
        os << "{\"kind\":\"bb\",\"index\":" << i << ",\"statements\":[";
        for (size_t k = 0; k < bb.statements.size(); ++k) {
            if (k) os << ",";
            os << "\"" << escape_json(bb.statements[k]) << "\"";
        }
        os << "],\"term\":";
        emit_terminator(os, bb.term);
        os << "}";
    }
    os << "]}";
}

}  // namespace cpp2model