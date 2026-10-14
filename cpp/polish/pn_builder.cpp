#include "pn_builder.h"

namespace cpp2model {

void PolishBuilder::build(const TreeModel& ast) {
    m_tree = &ast;
    m_out.clear();
    if (ast.nodes.empty()) return;
    walk(0, m_out);
    if (ast.truncated) m_out += " TRUNCATED";
}

std::string PolishBuilder::tokenFor(const Node& n) const {
    return n.detail.empty() ? n.kind : n.detail;
}

bool PolishBuilder::isVariableArity(const std::string& kind) const {
    return kind == "CompoundStmt"
        || kind == "DeclStmt"
        || kind == "CallExpr"
        || kind == "CXXOperatorCallExpr"
        || kind == "CXXConstructExpr"
        || kind == "InitListExpr";
}

void PolishBuilder::walk(int idx, std::string& out) {
    const Node& n = m_tree->nodes[idx];

    if (isVariableArity(n.kind)) {
        out += "(blk";
        out += std::to_string(n.children.size());
        for (int c : n.children) {
            out += ' ';
            walk(c, out);
        }
        out += ')';
        return;
    }

    out += tokenFor(n);
    for (int c : n.children) {
        out += ' ';
        walk(c, out);
    }
}

}  // namespace cpp2model