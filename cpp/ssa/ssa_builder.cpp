#include "ssa_builder.h"

#include <cassert>
#include <cctype>
#include <sstream>

namespace cpp2model {

namespace {

std::vector<std::string> split_ws(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!cur.empty()) { out.push_back(std::move(cur)); cur.clear(); }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(std::move(cur));
    return out;
}

}  // namespace

bool SsaBuilder::is_temp(const std::string& name) {
    return name.size() >= 6 && name.compare(0, 6, "__temp") == 0;
}

bool SsaBuilder::is_name_token(const std::string& tok) {
    if (tok.empty()) return false;
    unsigned char c0 = static_cast<unsigned char>(tok[0]);
    if (!(std::isalpha(c0) || tok[0] == '_')) return false;
    for (char c : tok) {
        unsigned char u = static_cast<unsigned char>(c);
        if (!(std::isalnum(u) || c == '_')) return false;
    }
    return true;
}

bool SsaBuilder::is_unary_op(const std::string& tok) {
    return tok == "!" || tok == "~" || tok == "-" || tok == "+"
        || tok == "*" || tok == "&";
}

bool SsaBuilder::is_special_unary(const std::string& tok) {
    return tok == "sizeof" || tok == "alignof";
}

bool SsaBuilder::is_binary_op(const std::string& tok) {
    static const char* const kOps[] = {
        "+", "-", "*", "/", "%",
        "<<", ">>", "&", "|", "^",
        "<", ">", "<=", ">=", "==", "!=",
        "&&", "||",
    };
    for (const char* op : kOps) if (tok == op) return true;
    return false;
}

TacOp SsaBuilder::parse_tac(const std::string& stmt) {
    TacOp out;
    const auto eq = stmt.find(" = ");
    assert(eq != std::string::npos && "TAC statement must contain ' = '");

    out.dst = stmt.substr(0, eq);

    const std::string rhs = stmt.substr(eq + 3);
    const std::vector<std::string> toks = split_ws(rhs);
    assert(!toks.empty() && "TAC RHS must not be empty");

    if (toks.size() == 1) {
        out.op = "=";
        out.args.push_back(toks[0]);
        return out;
    }

    if (toks.size() == 2) {
        assert((is_unary_op(toks[0]) || is_special_unary(toks[0]))
               && "two-token TAC RHS must be a unary operator");
        out.op = toks[0];
        out.args.push_back(toks[1]);
        return out;
    }

    if (toks.size() == 3) {
        assert((is_binary_op(toks[1]) || toks[1] == "=")
               && "three-token TAC RHS must be binary");
        out.op = toks[1];
        out.args.push_back(toks[0]);
        out.args.push_back(toks[2]);
        return out;
    }

    assert(false && "unexpected TAC RHS arity");
    return out;
}

ExprFragment SsaBuilder::parse_expr_fragment(const std::string& expr) {
    ExprFragment out;
    const std::vector<std::string> toks = split_ws(expr);
    assert(!toks.empty() && "terminator expression must not be empty");

    if (toks.size() == 1) {
        out.args.push_back(toks[0]);
        return out;
    }
    if (toks.size() == 3) {
        assert(is_binary_op(toks[1]) && "fragment middle token must be binary");
        out.op = toks[1];
        out.args.push_back(toks[0]);
        out.args.push_back(toks[2]);
        return out;
    }
    assert(false && "unexpected terminator expression arity");
    return out;
}

void SsaBuilder::computeEdges() {
    const int n = static_cast<int>(m_ssa.blocks.size());
    for (int i = 0; i < n; ++i) {
        const Terminator& t = m_ssa.blocks[i].term;
        auto add_edge = [&](int target) {
            assert(target >= 0 && target < n && "terminator points out of range");
            m_ssa.blocks[i].succs.push_back(target);
            m_ssa.blocks[target].preds.push_back(i);
        };
        switch (t.kind) {
            case TermKind::Fallthrough:
                assert(false && "Fallthrough terminator survived into SSA");
                break;
            case TermKind::Jump:
                add_edge(t.target);
                break;
            case TermKind::Branch:
                add_edge(t.on_true);
                add_edge(t.on_false);
                break;
            case TermKind::Switch:
                for (const auto& c : t.cases) add_edge(c.second);
                add_edge(t.default_target);
                break;
            case TermKind::Return:
                break;
        }
    }
}

void SsaBuilder::parseBlocks(const Cfg& cfg) {
    m_ssa.func_decl = cfg.func_decl;
    m_ssa.blocks.clear();
    m_ssa.blocks.reserve(cfg.blocks.size());

    for (const BB& bb : cfg.blocks) {
        SsaBB out;
        out.term = bb.term;
        out.ops.reserve(bb.statements.size());
        for (const std::string& s : bb.statements) {
            out.ops.push_back(parse_tac(s));
        }
        m_ssa.blocks.push_back(std::move(out));
    }
}

void SsaBuilder::build(const Cfg& cfg) {
    parseBlocks(cfg);
    computeEdges();
}

} // namespace cpp2model