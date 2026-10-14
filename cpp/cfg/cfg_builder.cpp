#include "cfg_builder.h"
#include "cfg_utils.h"
#include <cassert>
#include <variant>

namespace cpp2model {

std::string CfgBuilder::makeTemp() {
  return "__temp" + std::to_string(m_temp_cnt++);
}

const Node &CfgBuilder::getNode(int i) const {
  assert(i >= 0 && i < static_cast<int>(m_tree.nodes.size()));
  return m_tree.nodes[i];
}

int CfgBuilder::newBlock() {
  m_cfg.blocks.push_back(BB{});
  return static_cast<int>(m_cfg.blocks.size() - 1);
}

void CfgBuilder::addToBlock(const std::string &stmt) {
  assert(m_current != -1 && m_current < static_cast<int>(m_cfg.blocks.size()));
  m_cfg.blocks[m_current].statements.push_back(stmt);
}

int CfgBuilder::branch(const std::string &cond, int on_true, int on_false) {
  Terminator t{};
  t.kind = TermKind::Branch;
  t.cond = cond;
  t.on_true = on_true;
  t.on_false = on_false;
  m_cfg.blocks[m_current].term = t;
  return -1;
}

void CfgBuilder::sealTo(int target) {
  BB &b = m_cfg.blocks[m_current];
  if (b.term.kind == TermKind::Fallthrough) {
    Terminator t{};
    t.kind = TermKind::Jump;
    t.target = target;
    b.term = t;
  }
  // if not - the block is already sealed (non-fallthrough is already handled)
}

bool CfgBuilder::isValueNode(const Node &n) const {
  return n.kind == "IntegerLiteral" || n.kind == "FloatingLiteral" ||
         n.kind == "CXXBoolLiteralExpr" || n.kind == "DeclRefExpr" ||
         n.kind == "StringLiteral" || n.kind == "CharacterLiteral" ||
         n.kind == "CXXNullPtrLiteralExpr" || n.kind == "CXXThisExpr";
}

bool CfgBuilder::isSpecialUnary(const std::string &op) const {
  return op == "sizeof" || op == "alignof";
}

bool CfgBuilder::isCompoundAssign(const std::string &op) const {
  static const char *const kCompound[] = {
      "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "|=", "^="};
  for (const char *c : kCompound) {
    if (op == c)
      return true;
  }
  return false;
}

// this function extracts binary operator from compound assign
std::string CfgBuilder::compoundBase(const std::string &op) const {
  return op.substr(0, op.size() - 1);
}

bool CfgBuilder::isPostfixUnary(const Node &n) const {
  if (n.range.empty() || n.children.empty())
    return false;
  const Node &operand = getNode(n.children.front());
  auto begin_of = [](const std::string &range) {
    std::string::size_type dash = range.find('-');
    return dash == std::string::npos ? range : range.substr(0, dash);
  };
  return begin_of(n.range) == begin_of(operand.range);
}

std::string CfgBuilder::walkExpr(int e, bool temp_needed = false,
                                 bool value_needed = true) {
  const Node &expr_node = getNode(e);

  if (isValueNode(expr_node))
    return expr_node.detail;

  if (expr_node.kind == "ParenExpr") {
    // '(*)' do nothing, as order is enforced by AST structure
    // this makes me question why it's in AST in the first place
    if (expr_node.children.empty())
      return "";
    return walkExpr(expr_node.children.front(), temp_needed, value_needed);
  }
  if (expr_node.kind == "UnaryOperator" ||
      expr_node.kind == "UnaryExprOrTypeTraitExpr")
    return walkUnary(e, temp_needed,
                     value_needed); // oooh allowing type trait expressions will
                                    // go so wrong i can feel it
  if (expr_node.kind == "BinaryOperator" ||
      expr_node.kind == "CompoundAssignOperator")
    return walkBinary(e, temp_needed, value_needed);

  // will support later
  return "";
}

std::string CfgBuilder::walkUnary(int e, bool temp_needed, bool value_needed) {
  const Node &expr_node = getNode(e);
  const std::string op = expr_node.detail;
  if (expr_node.children.empty()) {
    return op;
  }

  if (op == "++" || op == "--") {
    // these ops lower to assignment
    const std::string delta = (op == "++") ? "+" : "-";
    const std::string dest = walkExpr(expr_node.children.front(), true, true);
    if (dest.empty())
      return "";
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
  if (operand.empty())
    return "";
  const std::string expr = (isSpecialUnary(op) ? op + " " : op) + operand;
  if (!temp_needed)
    return expr;
  const std::string t = makeTemp();
  addToBlock(t + " = " + expr);
  return t;
}

std::string CfgBuilder::walkBinary(int e, bool temp_needed, bool value_needed) {
  (void)value_needed; // assignment lowering is identical whether or not
                      // the result value is consumed
  const Node &expr_node = getNode(e);
  const std::string op = expr_node.detail;
  assert(expr_node.children.size() == 2);
  const int lhs_i = expr_node.children.front();
  const int rhs_i = expr_node.children.back();

  if (op == "=") {
    // assignment is an expression that returns assigned value
    const std::string val = walkExpr(rhs_i, true, true);
    const std::string dest = walkExpr(lhs_i, true, true);
    if (dest.empty() || val.empty())
      return "";
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
    if (dest.empty() || val.empty())
      return "";
    addToBlock(dest + " = " + dest + " " + compoundBase(op) + " " + val);
    return dest;
  }

  const std::string lhs = walkExpr(lhs_i, true, true);
  const std::string rhs = walkExpr(rhs_i, true, true);
  if (lhs.empty() || rhs.empty())
    return "";
  const std::string expr = lhs + " " + op + " " + rhs;
  if (!temp_needed)
    return expr;
  const std::string t = makeTemp();
  addToBlock(t + " = " + expr);
  return t;
}

void CfgBuilder::walkStmt(int s) {
  const Node &stmt_node = getNode(s);

  if (stmt_node.kind == "CompoundStmt") {
    // le compound statement
    // isn't a new basic block
    for (int child_idx : stmt_node.children) {
      assert(child_idx >= 0 &&
             child_idx < static_cast<int>(m_tree.nodes.size()));
      walkStmt(child_idx);
    }
  } else if (stmt_node.kind == "DeclStmt") {
    for (int child_idx : stmt_node.children)
      walkStmt(child_idx);
  } else if (stmt_node.kind == "IfStmt") {
    walkIf(s);
  } else if (stmt_node.kind == "WhileStmt") {
    walkWhile(s);
  } else if (stmt_node.kind == "ForStmt") {
    walkFor(s);
  } else if (stmt_node.kind == "CXXForRangeStmt") {
    walkForEach(s);
  } else if (stmt_node.kind == "DoStmt") {
    walkDoWhile(s);
  } else if (stmt_node.kind == "BreakStmt") {
    walkBreak();
  } else if (stmt_node.kind == "ContinueStmt") {
    walkContinue();
  } else if (stmt_node.kind == "ReturnStmt") {
    walkReturn(s);
  } else if (stmt_node.kind == "GotoStmt") {
    walkGoto(s);
  } else if (stmt_node.kind == "LabelStmt") {
    walkLabel(s);
  } else if (stmt_node.kind == "SwitchStmt") {
    walkSwitch(s);
  } else if (stmt_node.kind == "CaseStmt") {
    walkCase(s);
  } else if (stmt_node.kind == "DefaultStmt") {
    walkDefault(s);
  } else if (stmt_node.kind == "Var") {
    if (!stmt_node.children.empty()) {
      const std::string lhs = stmt_node.detail;
      const std::string rhs = walkExpr(stmt_node.children.front());
      if (!lhs.empty() && !rhs.empty()) {
        addToBlock(lhs + " = " + rhs);
      }
    }
  } else if (stmt_node.cat == NodeCategory::Expression) {
    // a standalone expression used as a statement, e.g. "x = 5;", "y++;"
    // its value is discarded; only side effects reach the CFG
    // this also handles the stupid ',' operator
    walkExpr(s, false, false);
  }
}

void CfgBuilder::walkIf(int s) {
  const Node &if_node = getNode(s);
  assert(if_node.children.size() == 4);

  int init_i = if_node.children[0];
  int cond_i = if_node.children[1];
  int then_i = if_node.children[2];
  int else_i = if_node.children[3];

  if (getNode(init_i).kind != "EmptyInit") {
    walkStmt(init_i);
  }

  std::string cond = walkExpr(cond_i);

  int then_bb = newBlock();
  int join_bb = newBlock();

  bool has_else = (getNode(else_i).kind != "EmptyElse");
  int else_bb = has_else ? newBlock() : join_bb;

  branch(cond, then_bb, else_bb);

  m_current = then_bb;
  walkStmt(then_i);
  sealTo(join_bb);

  if (has_else) {
    m_current = else_bb;
    walkStmt(else_i);
    sealTo(join_bb);
  }

  m_current = join_bb;
}

void CfgBuilder::walkWhile(int s) {
  const Node &while_node = getNode(s);

  assert(while_node.children.size() == 3);

  int condvar_i = while_node.children[0];
  int cond_i = while_node.children[1];
  int body_i = while_node.children[2];

  bool has_condvar = getNode(condvar_i).kind != "NoConditionVar";
  bool has_body = getNode(body_i).kind != "NullStmt";

  int cond_bb = newBlock();
  int body_bb = has_body ? newBlock() : cond_bb;
  int join_bb = newBlock();

  sealTo(cond_bb);

  m_current = cond_bb;
  if (has_condvar) {
    walkStmt(condvar_i);
  }
  std::string c = walkExpr(cond_i);
  branch(c, body_bb, join_bb);

  m_break_targets.push_back(join_bb);    // breaks and false branch
  m_continue_targets.push_back(cond_bb); // continues and true branch

  if (has_body) {
    m_current = body_bb;
    walkStmt(body_i);
    sealTo(cond_bb); // backedge
  }

  m_break_targets.pop_back();
  m_continue_targets.pop_back();

  m_current = join_bb;
}

void CfgBuilder::walkFor(int s) {
  const Node &for_node = getNode(s);

  assert(for_node.children.size() == 4);

  int init_i = for_node.children[0];
  int cond_i = for_node.children[1];
  int incr_i = for_node.children[2];
  int body_i = for_node.children[3];

  bool has_init = (getNode(init_i).kind != "EmptyInit");
  bool has_cond = (getNode(cond_i).kind != "EmptyCond");
  bool has_incr = (getNode(incr_i).kind != "EmptyInc");
  bool has_body = (getNode(body_i).kind != "NullStmt");

  if (has_init) {
    walkStmt(init_i);
  }

  int cond_bb = newBlock();
  int incr_bb = has_incr ? newBlock() : cond_bb;
  int body_bb = has_body ? newBlock() : incr_bb;
  int join_bb = newBlock();

  sealTo(cond_bb);
  m_current = cond_bb;
  if (has_cond) {
    std::string c = walkExpr(cond_i);
    branch(c, body_bb, join_bb);
  } else {
    Terminator t{};
    t.kind = TermKind::Jump;
    t.target = body_bb;
    m_cfg.blocks[cond_bb].term = t;
  }

  m_break_targets.push_back(join_bb);
  m_continue_targets.push_back(incr_bb);

  if (has_body) {
    m_current = body_bb;
    walkStmt(body_i);
  }

  sealTo(incr_bb);

  if (has_incr) {
    m_current = incr_bb;
    walkExpr(incr_i);
    sealTo(cond_bb);
  }

  m_break_targets.pop_back();
  m_continue_targets.pop_back();

  m_current = join_bb;
}

void CfgBuilder::walkForEach(int s) {
  const Node &foreach_node = getNode(s);

  assert(foreach_node.children.size() == 7); // wow that's a lot

  int range_i = foreach_node.children[0];
  int begin_i = foreach_node.children[1];
  int end_i = foreach_node.children[2];
  int cond_i = foreach_node.children[3];
  int incr_i = foreach_node.children[4];
  int var_i = foreach_node.children[5];
  int body_i = foreach_node.children[6];

  walkStmt(range_i);
  walkStmt(begin_i);
  walkStmt(end_i);

  int cond_bb = newBlock();
  int body_bb = newBlock();
  int incr_bb = newBlock();
  int join_bb = newBlock();

  sealTo(cond_bb);
  m_current = cond_bb;
  std::string c = walkExpr(cond_i);
  branch(c, body_bb, join_bb);

  m_break_targets.push_back(join_bb);
  m_continue_targets.push_back(incr_bb);

  m_current = body_bb;
  walkStmt(var_i);
  walkStmt(body_i);

  sealTo(incr_bb);
  m_current = incr_bb;
  walkExpr(incr_i, false, false);
  sealTo(cond_bb);

  m_break_targets.pop_back();
  m_continue_targets.pop_back();

  m_current = join_bb;
}

void CfgBuilder::walkDoWhile(int s) {
  const Node &dowhile_node = getNode(s);

  assert(dowhile_node.children.size() == 2);

  int body_i = dowhile_node.children[0];
  int cond_i = dowhile_node.children[1];

  int cond_bb = newBlock();
  int join_bb = newBlock();

  m_break_targets.push_back(join_bb);
  m_continue_targets.push_back(cond_bb);

  sealTo(cond_bb);
  m_current = cond_bb;
  walkStmt(body_i);

  std::string c = walkExpr(cond_i);
  branch(c, cond_bb, join_bb);

  m_break_targets.pop_back();
  m_continue_targets.pop_back();

  m_current = join_bb;
}

void CfgBuilder::walkBreak() {
  assert(!m_break_targets.empty());
  sealTo(m_break_targets.back());
  m_current = newBlock();
}

void CfgBuilder::walkContinue() {
  assert(!m_continue_targets.empty());
  sealTo(m_continue_targets.back());
  m_current = newBlock();
}

void CfgBuilder::walkReturn(int s) {
  const Node &return_stmt = getNode(s);
  Terminator t{};
  t.kind = TermKind::Return;
  if (!return_stmt.children.empty()) {
    // from what i remember return stmt has no more than 1 child
    t.ret_val = walkExpr(return_stmt.children.front());
  }
  m_cfg.blocks[m_current].term = t;
  m_current = newBlock();
}

void CfgBuilder::walkLabel(int s) {
  const Node &label_node = getNode(s);

  // if current block empty and unsealed, label adopts it
  // otherwise new block is created
  BB &cur = m_cfg.blocks[m_current];
  if (!cur.statements.empty() || cur.term.kind != TermKind::Fallthrough) {
    int label_bb = newBlock();
    sealTo(label_bb);
    m_current = label_bb;
  }

  m_label_block[label_node.detail] = m_current;
  for (int child_idx : label_node.children) {
    walkStmt(child_idx);
  }
}

void CfgBuilder::walkGoto(int s) {
  const Node &goto_node = getNode(s);
  Terminator t{};
  t.kind = TermKind::Jump;
  t.target = -1;
  m_cfg.blocks[m_current].term = t;
  m_pending_gotos.push_back({m_current, goto_node.detail});
  m_current = newBlock();
}

void CfgBuilder::walkSwitch(int s) {
  const Node &sw_node = getNode(s);
  assert(sw_node.children.size() == 3);

  int init_i = sw_node.children[0];
  int cond_i = sw_node.children[1];
  int body_i = sw_node.children[2];

  bool has_init = getNode(init_i).kind != "EmptyInit";

  if (has_init) {
    walkStmt(init_i);
  }

  const std::string val = walkExpr(cond_i);

  int switch_bb = m_current;

  Terminator t{};
  t.kind = TermKind::Switch;
  t.switch_val = val;
  t.default_target = -1;
  m_cfg.blocks[switch_bb].term = t;

  int join_bb = newBlock();

  m_current = newBlock();

  m_break_targets.push_back(join_bb);
  m_switch_targets.push_back(switch_bb);

  walkStmt(body_i);
  sealTo(join_bb);

  m_switch_targets.pop_back();
  m_break_targets.pop_back();

  Terminator &final_term = m_cfg.blocks[switch_bb].term;
  if (final_term.default_target < 0) {
    final_term.default_target = join_bb;
  }

  m_current = join_bb;
}

void CfgBuilder::walkCase(int s) {
  const Node &case_node = getNode(s);

  assert(!m_switch_targets.empty());
  int switch_bb = m_switch_targets.back();

  BB &cur = m_cfg.blocks[m_current];
  if (!cur.statements.empty() || cur.term.kind != TermKind::Fallthrough) {
    int case_bb = newBlock();
    sealTo(case_bb);
    m_current = case_bb;
  }

  long long value = 0;
  bool parsed = false;
  if (!case_node.detail.empty() && case_node.detail != "?") {
    char *end = nullptr;
    value = std::strtoll(case_node.detail.c_str(), &end, 10);
    parsed = (end != case_node.detail.c_str()) && (*end == '\0');
  }

  if (parsed) {
    m_cfg.blocks[switch_bb].term.cases.push_back({value, m_current});
  }

  for (int child_idx : case_node.children)
    walkStmt(child_idx);
}

void CfgBuilder::walkDefault(int s) {
  const Node &default_node = getNode(s);

  assert(!m_switch_targets.empty());
  int switch_bb = m_switch_targets.back();

  BB &cur = m_cfg.blocks[m_current];
  if (!cur.statements.empty() || cur.term.kind != TermKind::Fallthrough) {
    int default_bb = newBlock();
    sealTo(default_bb);
    m_current = default_bb;
  }

  m_cfg.blocks[switch_bb].term.default_target = m_current;

  for (int child_idx : default_node.children)
    walkStmt(child_idx);
}

void CfgBuilder::patchGotos() {
  for (const auto &pg : m_pending_gotos) {
    auto it = m_label_block.find(pg.label);
    if (it != m_label_block.end()) {
      m_cfg.blocks[pg.block].term.target = it->second;
    }
  }
  m_pending_gotos.clear();
  m_label_block.clear();
}

void CfgBuilder::walkFunction(int f) {
  const Node &func_node = getNode(f);

  std::string func_name = func_node.detail;
  std::vector<std::string> func_params;
  for (int child_idx : func_node.children) {
    const Node &child = getNode(child_idx);
    if (child.kind != "ParmVar")
      break;
    func_params.push_back(child.detail);
  }
  m_cfg.func_decl = stringify_func_decl(func_name, func_params);

  // last child of function with body is always a compound statement
  int comp_stmt_idx = func_node.children.back();
  if (getNode(comp_stmt_idx).kind != "CompoundStmt")
    return;
  m_cfg.blocks.push_back(BB{});
  m_current = m_cfg.blocks.size() - 1;
  walkStmt(comp_stmt_idx);

  {
    BB &b = m_cfg.blocks[m_current];
    if (b.term.kind == TermKind::Fallthrough) {
      // implicit return
      // usually happens in main or void functions
      Terminator t{};
      t.kind = TermKind::Return;
      b.term = t;
    }
  }

  patchGotos();
}

void CfgBuilder::build(const TreeModel &ast) {
  if (ast.nodes.empty())
    return;
  m_tree = ast;
  const Node &root = getNode(0);
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