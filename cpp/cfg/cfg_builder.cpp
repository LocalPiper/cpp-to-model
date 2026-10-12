#include "cfg_builder.h"
#include "cfg_utils.h"
#include <cassert>
#include <variant>

namespace cpp2model {

std::string CfgBuilder::makeTemp() {
    return "__temp" + std::to_string(m_temp_cnt++);
}

const Node& CfgBuilder::getNode(int i) const {
    assert(i >= 0 && i < static_cast<int>(m_tree.nodes.size()));
    return m_tree.nodes[i];
}

int CfgBuilder::newBlock() {
    m_cfg.blocks.push_back(BB{});
    return static_cast<int>(m_cfg.blocks.size() - 1);
}

void CfgBuilder::addToBlock(const std::string& stmt) {
    assert(m_current != -1 && m_current < static_cast<int>(m_cfg.blocks.size()));
    m_cfg.blocks[m_current].statements.push_back(stmt);
}

int CfgBuilder::branch(const std::string& cond, int on_true, int on_false) {
    Terminator t{};
    t.kind = TermKind::Branch;
    t.cond = cond;
    t.on_true = on_true;
    t.on_false = on_false;
    m_cfg.blocks[m_current].term = t;
    return -1;
}

void CfgBuilder::sealTo(int target) {
    BB& b = m_cfg.blocks[m_current];
    if (b.term.kind == TermKind::Fallthrough) {
        Terminator t{};
        t.kind = TermKind::Jump;
        t.target = target;
        b.term = t;
    }
    // if not - the block is already sealed (non-fallthrough is already handled)
}


bool CfgBuilder::isValueNode(const Node& n) const {
    return n.kind == "IntegerLiteral"
        || n.kind == "FloatingLiteral"
        || n.kind == "CXXBoolLiteralExpr"
        || n.kind == "DeclRefExpr"
        || n.kind == "StringLiteral"
        || n.kind == "CharacterLiteral"
        || n.kind == "CXXNullPtrLiteralExpr"
        || n.kind == "CXXThisExpr";
}

bool CfgBuilder::isSpecialUnary(const std::string& op) const {
    return op == "sizeof" || op == "alignof";
}

bool CfgBuilder::isCompoundAssign(const std::string& op) const {
    static const char* const kCompound[] = { "+=", "-=", "*=", "/=", "%=",
                                             "<<=", ">>=", "&=", "|=", "^=" };
    for (const char* c : kCompound) {
        if (op == c) return true;
    }
    return false;
}

// this function extracts binary operator from compound assign
std::string CfgBuilder::compoundBase(const std::string& op) const {
    return op.substr(0, op.size() - 1);
}

bool CfgBuilder::isPostfixUnary(const Node& n) const {
    if (n.range.empty() || n.children.empty()) return false;
    const Node& operand = getNode(n.children.front());
    auto begin_of = [](const std::string& range) {
        std::string::size_type dash = range.find('-');
        return dash == std::string::npos ? range : range.substr(0, dash);
    };
    return begin_of(n.range) == begin_of(operand.range);
}

std::string CfgBuilder::walkExpr(int e, bool temp_needed = false,
                                 bool value_needed = true) {
    const Node& expr_node = getNode(e);

    if (isValueNode(expr_node)) return expr_node.detail;

    if (expr_node.kind == "ParenExpr") {
        // '(*)' do nothing, as order is enforced by AST structure
        // this makes me question why it's in AST in the first place
        if (expr_node.children.empty()) return "";
        return walkExpr(expr_node.children.front(), temp_needed, value_needed);
    }
    if (expr_node.kind == "UnaryOperator" || expr_node.kind == "UnaryExprOrTypeTraitExpr")
        return walkUnary(e, temp_needed, value_needed); // oooh allowing type trait expressions will go so wrong i can feel it
    if (expr_node.kind == "BinaryOperator" || expr_node.kind == "CompoundAssignOperator")
        return walkBinary(e, temp_needed, value_needed);

    // will support later
    return "";
}

std::string CfgBuilder::walkUnary(int e, bool temp_needed, bool value_needed) {
    const Node& expr_node = getNode(e);
    const std::string op = expr_node.detail;
    assert(!expr_node.children.empty());

    if (op == "++" || op == "--") {
        // these ops lower to assignment
        const std::string delta = (op == "++") ? "+" : "-";
        const std::string dest = walkExpr(expr_node.children.front(), true, true);
        if (dest.empty()) return "";
        if (isPostfixUnary(expr_node) && value_needed) {
            // store, update, assign old
            const std::string t = makeTemp();
            addToBlock(t + " = " + dest);
            addToBlock(dest + " = " + dest + " " + delta + " 1");
            return t;
        }
        addToBlock(dest + " = " + dest + " " + delta + " 1");
        return value_needed ? dest : "";
    }

    const std::string operand = walkExpr(expr_node.children.front(), true, true);
    if (operand.empty()) return "";
    const std::string expr = (isSpecialUnary(op) ? op + " " : op) + operand;
    if (!temp_needed) return expr;
    const std::string t = makeTemp();
    addToBlock(t + " = " + expr);
    return t;
}

std::string CfgBuilder::walkBinary(int e, bool temp_needed, bool value_needed) {
    (void)value_needed;  // assignment lowering is identical whether or not
                         // the result value is consumed
    const Node& expr_node = getNode(e);
    const std::string op = expr_node.detail;
    assert(expr_node.children.size() == 2);
    const int lhs_i = expr_node.children.front();
    const int rhs_i = expr_node.children.back();

    if (op == "=") {
        // assignment is an expression that returns assigned value
        const std::string val = walkExpr(rhs_i, true, true);
        const std::string dest = walkExpr(lhs_i, true, true);
        if (dest.empty() || val.empty()) return "";
        addToBlock(dest + " = " + val);
        return val;
    }

    if (op == ",") {
        // comma operator
        (void)walkExpr(lhs_i, false, false);
        return walkExpr(rhs_i, temp_needed, value_needed);
    }

    if (isCompoundAssign(op)) {
        // "a OP= b" -> "a = a OP b";
        const std::string dest = walkExpr(lhs_i, true, true);
        const std::string val = walkExpr(rhs_i, true, true);
        if (dest.empty() || val.empty()) return "";
        addToBlock(dest + " = " + dest + " " + compoundBase(op) + " " + val);
        return dest;
    }

    const std::string lhs = walkExpr(lhs_i, true, true);
    const std::string rhs = walkExpr(rhs_i, true, true);
    if (lhs.empty() || rhs.empty()) return "";
    const std::string expr = lhs + " " + op + " " + rhs;
    if (!temp_needed) return expr;
    const std::string t = makeTemp();
    addToBlock(t + " = " + expr);
    return t;
}

void CfgBuilder::walkStmt(int s) {
    const Node& stmt_node = getNode(s);

    if (stmt_node.kind == "CompoundStmt") {
        // le compound statement
        // isn't a new basic block
        for (int child_idx : stmt_node.children) {
            assert(child_idx >= 0 && child_idx < static_cast<int>(m_tree.nodes.size()));
            walkStmt(child_idx);
        }
    } else if (stmt_node.kind == "DeclStmt") {
        // declaration statement; each child is a var declaration
        // e.g. "int x = 1;" or "int x = 1, y = x + 1, z;"
        for (int child_idx : stmt_node.children) {
            const Node& var_node = getNode(child_idx);
            if (var_node.kind != "Var") continue;
            // declaration without an initializer has no appearance in the CFG
            if (var_node.children.empty()) continue;
            const std::string lhs = var_node.detail;
            const std::string rhs = walkExpr(var_node.children.front(), false, true);
            if (lhs.empty() || rhs.empty()) continue;
            addToBlock(lhs + " = " + rhs);
        }
    } else if (stmt_node.kind == "IfStmt") {
        walkIf(s);
    } else if (stmt_node.kind == "WhileStmt") {
        walkWhile(s);
    } else if (stmt_node.cat == NodeCategory::Expression) {
        // a standalone expression used as a statement, e.g. "x = 5;", "y++;"
        // its value is discarded; only side effects reach the CFG
        // this also handles the stupid ',' operator
        walkExpr(s, false, false);
    }
}


void CfgBuilder::walkIf(int s) {
    const Node& if_node = getNode(s);

    std::string cond = walkExpr(if_node.children.front(), true, true);

    int then_bb = newBlock();
    int join_bb = newBlock();

    bool has_else = (if_node.children.size() > 2);
    int else_bb = has_else ? newBlock() : join_bb;

    branch(cond, then_bb, else_bb);

    // then branch is guaranteed in any if statement
    m_current = then_bb;
    walkStmt(if_node.children[1]);
    sealTo(join_bb);

    if (has_else) {
        m_current = else_bb;
        walkStmt(if_node.children[2]);
        sealTo(join_bb);
    }

    m_current = join_bb;
}

void CfgBuilder::walkWhile(int s) {
    const Node& while_node = getNode(s);

    int cond_bb = newBlock();
    int body_bb = newBlock();
    int join_bb = newBlock();

    sealTo(cond_bb);

    m_current = cond_bb;
    std::string c = walkExpr(while_node.children.front());
    branch(c, body_bb, join_bb);

    m_break_targets.push_back(join_bb); // breaks and false branch
    m_continue_targets.push_back(cond_bb); // continues and true branch
    
    m_current = body_bb;
    walkStmt(while_node.children[1]);
    sealTo(cond_bb); // backedge

    m_break_targets.pop_back();
    m_continue_targets.pop_back();

    m_current = join_bb;
}

void CfgBuilder::walkFunction(int f) {
    const Node& func_node = getNode(f);

    std::string func_name = func_node.detail;
    std::vector<std::string> func_params;
    for (int child_idx : func_node.children) {
        const Node& child = getNode(child_idx);
        if (child.kind != "ParmVar") break;
        func_params.push_back(child.detail);
    }
    m_cfg.func_decl = stringify_func_decl(func_name, func_params);

    // last child of function with body is always a compound statement
    int comp_stmt_idx = func_node.children.back();
    if (getNode(comp_stmt_idx).kind != "CompoundStmt") return;
    m_cfg.blocks.push_back(BB{});
    m_current = m_cfg.blocks.size() - 1;
    walkStmt(comp_stmt_idx);
}

void CfgBuilder::build(const TreeModel &ast) {
    if (ast.nodes.empty()) return;
    m_tree = ast;
    const Node& root = getNode(0);
    // for now, we assume that our ast contains only 1 function definition
    bool function_found = false;
    for (int child_idx : root.children) {
        if (getNode(child_idx).kind == "Function") {
            assert(!function_found);
            function_found = true;
            walkFunction(child_idx);
        }
    }
}

} // namespace cpp2model