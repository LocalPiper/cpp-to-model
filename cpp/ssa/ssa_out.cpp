#include "ssa_out.h"

#include <ostream>
#include <cassert>

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

void emit_tac(std::ostream& os, const TacOp& op) {
    os << "\"" << escape_json(op.dst) << " = ";
    if (op.op == "=") {
        assert(op.args.size() == 1);
        os << escape_json(op.args[0]);
    } else if (op.args.size() == 1) {
        const bool spaced = (op.op == "sizeof" || op.op == "alignof");
        os << escape_json(op.op) << (spaced ? " " : "") << escape_json(op.args[0]);
    } else if (op.args.size() == 2) {
        os << escape_json(op.args[0]) << " " << escape_json(op.op)
           << " " << escape_json(op.args[1]);
    } else {
        assert(false && "TacOp has unsupported arity");
    }
    os << "\"";
}

void emit_phi(std::ostream& os, const PhiNode& phi) {
    os << "{\"var\":\"" << escape_json(phi.var) << "\",\"incoming\":[";
    for (size_t i = 0; i < phi.incoming.size(); ++i) {
        if (i) os << ",";
        os << "{\"value\":\"" << escape_json(phi.incoming[i].value) << "\""
           << ",\"pred\":" << phi.incoming[i].pred_block << "}";
    }
    os << "]}";
}

}  // namespace

void emit_ssa_json(std::ostream& os, const SsaCfg& cfg) {
    os << "{\"func_decl\":\"" << escape_json(cfg.func_decl) << "\",\"blocks\":[";
    for (size_t i = 0; i < cfg.blocks.size(); ++i) {
        if (i) os << ",";
        const SsaBB& bb = cfg.blocks[i];

        os << "{\"kind\":\"bb\",\"index\":" << i;

        if (i < cfg.rpo_num.size()) {
            os << ",\"rpo_num\":" << cfg.rpo_num[i];
        }
        if (i < cfg.idom.size()) {
            os << ",\"idom\":" << cfg.idom[i];
        }

        os << ",\"preds\":[";
        for (size_t k = 0; k < bb.preds.size(); ++k) {
            if (k) os << ",";
            os << bb.preds[k];
        }
        os << "],\"succs\":[";
        for (size_t k = 0; k < bb.succs.size(); ++k) {
            if (k) os << ",";
            os << bb.succs[k];
        }
        os << "]";

        os << ",\"phis\":[";
        for (size_t k = 0; k < bb.phis.size(); ++k) {
            if (k) os << ",";
            emit_phi(os, bb.phis[k]);
        }
        os << "]";

        os << ",\"statements\":[";
        for (size_t k = 0; k < bb.ops.size(); ++k) {
            if (k) os << ",";
            emit_tac(os, bb.ops[k]);
        }
        os << "]";

        os << ",\"term\":";
        emit_terminator(os, bb.term);
        os << "}";
    }
    os << "]}";
}

} // namespace cpp2model