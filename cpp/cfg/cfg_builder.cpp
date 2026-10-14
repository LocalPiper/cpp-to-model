#include "cfg_builder.h"

#include "cfg_utils.h"

#include <cassert>
#include <cstdlib>
#include <variant>

namespace cpp2model {

std::string CfgBuilder::makeTemp() {
  return "__temp" + std::to_string(TempCount++);
}

const Node &CfgBuilder::getNode(int i) const {
  assert(i >= 0 && i < static_cast<int>(Tree->nodes.size()));
  return Tree->nodes[i];
}

int CfgBuilder::newBlock() {
  CfgModel.Blocks.push_back(BB{});
  return static_cast<int>(CfgModel.Blocks.size() - 1);
}

void CfgBuilder::addToBlock(const std::string &stmt) {
  assert(Current != -1 && Current < static_cast<int>(CfgModel.Blocks.size()));
  CfgModel.Blocks[Current].Statements.push_back(stmt);
}

void CfgBuilder::branch(const std::string &cond, int onTrue, int onFalse) {
  Terminator t{};
  t.Kind = TermKind::Branch;
  t.Cond = cond;
  t.OnTrue = onTrue;
  t.OnFalse = onFalse;
  CfgModel.Blocks[Current].Term = t;
}

void CfgBuilder::sealTo(int target) {
  BB &b = CfgModel.Blocks[Current];
  if (b.Term.Kind == TermKind::Fallthrough) {
    Terminator t{};
    t.Kind = TermKind::Jump;
    t.Target = target;
    b.Term = t;
  }
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
  for (const char *c : kCompound)
    if (op == c)
      return true;
  return false;
}

std::string CfgBuilder::compoundBase(const std::string &op) const {
  return op.substr(0, op.size() - 1);
}

bool CfgBuilder::isPostfixUnary(const Node &n) const {
  if (n.range.empty() || n.children.empty())
    return false;
  const Node &operand = getNode(n.children.front());
  auto beginOf = [](const std::string &range) {
    std::string::size_type dash = range.find('-');
    return dash == std::string::npos ? range : range.substr(0, dash);
  };
  return beginOf(n.range) == beginOf(operand.range);
}

std::string CfgBuilder::walkExpr(int e, bool tempNeeded, bool valueNeeded) {
  const Node &exprNode = getNode(e);

  if (isValueNode(exprNode))
    return exprNode.detail;

  if (exprNode.kind == "ParenExpr") {
    if (exprNode.children.empty())
      return "";
    return walkExpr(exprNode.children.front(), tempNeeded, valueNeeded);
  }
  if (exprNode.kind == "UnaryOperator" ||
      exprNode.kind == "UnaryExprOrTypeTraitExpr")
    return walkUnary(e, tempNeeded, valueNeeded);
  if (exprNode.kind == "BinaryOperator" ||
      exprNode.kind == "CompoundAssignOperator")
    return walkBinary(e, tempNeeded, valueNeeded);

  return "";
}

std::string CfgBuilder::walkUnary(int e, bool tempNeeded, bool valueNeeded) {
  const Node &exprNode = getNode(e);
  const std::string op = exprNode.detail;
  if (exprNode.children.empty())
    return op;

  if (op == "++" || op == "--") {
    const std::string delta = (op == "++") ? "+" : "-";
    const std::string dest = walkExpr(exprNode.children.front(), true, true);
    if (dest.empty())
      return "";
    if (isPostfixUnary(exprNode) && valueNeeded) {
      const std::string t = makeTemp();
      addToBlock(t + " = " + dest);
      addToBlock(dest + " = " + dest + " " + delta + " 1");
      return t;
    }
    addToBlock(dest + " = " + dest + " " + delta + " 1");
    return valueNeeded ? dest : "";
  }

  const std::string operand = walkExpr(exprNode.children.front(), true, true);
  if (operand.empty())
    return "";
  const std::string expr = (isSpecialUnary(op) ? op + " " : op) + operand;
  if (!tempNeeded)
    return expr;
  const std::string t = makeTemp();
  addToBlock(t + " = " + expr);
  return t;
}

std::string CfgBuilder::walkBinary(int e, bool tempNeeded, bool valueNeeded) {
  (void)valueNeeded;
  const Node &exprNode = getNode(e);
  const std::string op = exprNode.detail;
  assert(exprNode.children.size() == 2);
  const int lhsI = exprNode.children.front();
  const int rhsI = exprNode.children.back();

  if (op == "=") {
    const std::string val = walkExpr(rhsI, true, true);
    const std::string dest = walkExpr(lhsI, true, true);
    if (dest.empty() || val.empty())
      return "";
    addToBlock(dest + " = " + val);
    return val;
  }

  if (op == ",") {
    (void)walkExpr(lhsI, false, false);
    return walkExpr(rhsI, tempNeeded, valueNeeded);
  }

  if (isCompoundAssign(op)) {
    const std::string dest = walkExpr(lhsI, true, true);
    const std::string val = walkExpr(rhsI, true, true);
    if (dest.empty() || val.empty())
      return "";
    addToBlock(dest + " = " + dest + " " + compoundBase(op) + " " + val);
    return dest;
  }

  const std::string lhs = walkExpr(lhsI, true, true);
  const std::string rhs = walkExpr(rhsI, true, true);
  if (lhs.empty() || rhs.empty())
    return "";
  const std::string expr = lhs + " " + op + " " + rhs;
  if (!tempNeeded)
    return expr;
  const std::string t = makeTemp();
  addToBlock(t + " = " + expr);
  return t;
}

void CfgBuilder::walkStmt(int s) {
  const Node &stmtNode = getNode(s);

  if (stmtNode.kind == "CompoundStmt") {
    for (int childIdx : stmtNode.children)
      walkStmt(childIdx);
  } else if (stmtNode.kind == "DeclStmt") {
    for (int childIdx : stmtNode.children)
      walkStmt(childIdx);
  } else if (stmtNode.kind == "IfStmt") {
    walkIf(s);
  } else if (stmtNode.kind == "WhileStmt") {
    walkWhile(s);
  } else if (stmtNode.kind == "ForStmt") {
    walkFor(s);
  } else if (stmtNode.kind == "CXXForRangeStmt") {
    walkForEach(s);
  } else if (stmtNode.kind == "DoStmt") {
    walkDoWhile(s);
  } else if (stmtNode.kind == "BreakStmt") {
    walkBreak();
  } else if (stmtNode.kind == "ContinueStmt") {
    walkContinue();
  } else if (stmtNode.kind == "ReturnStmt") {
    walkReturn(s);
  } else if (stmtNode.kind == "GotoStmt") {
    walkGoto(s);
  } else if (stmtNode.kind == "LabelStmt") {
    walkLabel(s);
  } else if (stmtNode.kind == "SwitchStmt") {
    walkSwitch(s);
  } else if (stmtNode.kind == "CaseStmt") {
    walkCase(s);
  } else if (stmtNode.kind == "DefaultStmt") {
    walkDefault(s);
  } else if (stmtNode.kind == "Var") {
    if (!stmtNode.children.empty()) {
      const std::string lhs = stmtNode.detail;
      const std::string rhs = walkExpr(stmtNode.children.front());
      if (!lhs.empty() && !rhs.empty())
        addToBlock(lhs + " = " + rhs);
    }
  } else if (stmtNode.cat == NodeCategory::Expression) {
    walkExpr(s, false, false);
  }
}

void CfgBuilder::walkIf(int s) {
  const Node &ifNode = getNode(s);
  assert(ifNode.children.size() == 4);

  int initI = ifNode.children[0];
  int condI = ifNode.children[1];
  int thenI = ifNode.children[2];
  int elseI = ifNode.children[3];

  if (getNode(initI).kind != "EmptyInit")
    walkStmt(initI);

  std::string cond = walkExpr(condI);

  int thenBB = newBlock();
  int joinBB = newBlock();

  bool hasElse = (getNode(elseI).kind != "EmptyElse");
  int elseBB = hasElse ? newBlock() : joinBB;

  branch(cond, thenBB, elseBB);

  Current = thenBB;
  walkStmt(thenI);
  sealTo(joinBB);

  if (hasElse) {
    Current = elseBB;
    walkStmt(elseI);
    sealTo(joinBB);
  }

  Current = joinBB;
}

void CfgBuilder::walkWhile(int s) {
  const Node &whileNode = getNode(s);
  assert(whileNode.children.size() == 3);

  int condvarI = whileNode.children[0];
  int condI = whileNode.children[1];
  int bodyI = whileNode.children[2];

  bool hasCondvar = getNode(condvarI).kind != "NoConditionVar";
  bool hasBody = getNode(bodyI).kind != "NullStmt";

  int condBB = newBlock();
  int bodyBB = hasBody ? newBlock() : condBB;
  int joinBB = newBlock();

  sealTo(condBB);

  Current = condBB;
  if (hasCondvar)
    walkStmt(condvarI);
  std::string c = walkExpr(condI);
  branch(c, bodyBB, joinBB);

  BreakTargets.push_back(joinBB);
  ContinueTargets.push_back(condBB);

  if (hasBody) {
    Current = bodyBB;
    walkStmt(bodyI);
    sealTo(condBB);
  }

  BreakTargets.pop_back();
  ContinueTargets.pop_back();

  Current = joinBB;
}

void CfgBuilder::walkFor(int s) {
  const Node &forNode = getNode(s);
  assert(forNode.children.size() == 4);

  int initI = forNode.children[0];
  int condI = forNode.children[1];
  int incrI = forNode.children[2];
  int bodyI = forNode.children[3];

  bool hasInit = (getNode(initI).kind != "EmptyInit");
  bool hasCond = (getNode(condI).kind != "EmptyCond");
  bool hasIncr = (getNode(incrI).kind != "EmptyInc");
  bool hasBody = (getNode(bodyI).kind != "NullStmt");

  if (hasInit)
    walkStmt(initI);

  int condBB = newBlock();
  int incrBB = hasIncr ? newBlock() : condBB;
  int bodyBB = hasBody ? newBlock() : incrBB;
  int joinBB = newBlock();

  sealTo(condBB);
  Current = condBB;
  if (hasCond) {
    std::string c = walkExpr(condI);
    branch(c, bodyBB, joinBB);
  } else {
    Terminator t{};
    t.Kind = TermKind::Jump;
    t.Target = bodyBB;
    CfgModel.Blocks[condBB].Term = t;
  }

  BreakTargets.push_back(joinBB);
  ContinueTargets.push_back(incrBB);

  if (hasBody) {
    Current = bodyBB;
    walkStmt(bodyI);
  }

  sealTo(incrBB);

  if (hasIncr) {
    Current = incrBB;
    walkExpr(incrI);
    sealTo(condBB);
  }

  BreakTargets.pop_back();
  ContinueTargets.pop_back();

  Current = joinBB;
}

void CfgBuilder::walkForEach(int s) {
  const Node &foreachNode = getNode(s);
  assert(foreachNode.children.size() == 7);

  int rangeI = foreachNode.children[0];
  int beginI = foreachNode.children[1];
  int endI = foreachNode.children[2];
  int condI = foreachNode.children[3];
  int incrI = foreachNode.children[4];
  int varI = foreachNode.children[5];
  int bodyI = foreachNode.children[6];

  walkStmt(rangeI);
  walkStmt(beginI);
  walkStmt(endI);

  int condBB = newBlock();
  int bodyBB = newBlock();
  int incrBB = newBlock();
  int joinBB = newBlock();

  sealTo(condBB);
  Current = condBB;
  std::string c = walkExpr(condI);
  branch(c, bodyBB, joinBB);

  BreakTargets.push_back(joinBB);
  ContinueTargets.push_back(incrBB);

  Current = bodyBB;
  walkStmt(varI);
  walkStmt(bodyI);

  sealTo(incrBB);
  Current = incrBB;
  walkExpr(incrI, false, false);
  sealTo(condBB);

  BreakTargets.pop_back();
  ContinueTargets.pop_back();

  Current = joinBB;
}

void CfgBuilder::walkDoWhile(int s) {
  const Node &dowhileNode = getNode(s);
  assert(dowhileNode.children.size() == 2);

  int bodyI = dowhileNode.children[0];
  int condI = dowhileNode.children[1];

  int condBB = newBlock();
  int joinBB = newBlock();

  BreakTargets.push_back(joinBB);
  ContinueTargets.push_back(condBB);

  sealTo(condBB);
  Current = condBB;
  walkStmt(bodyI);

  std::string c = walkExpr(condI);
  branch(c, condBB, joinBB);

  BreakTargets.pop_back();
  ContinueTargets.pop_back();

  Current = joinBB;
}

void CfgBuilder::walkBreak() {
  assert(!BreakTargets.empty());
  sealTo(BreakTargets.back());
  Current = newBlock();
}

void CfgBuilder::walkContinue() {
  assert(!ContinueTargets.empty());
  sealTo(ContinueTargets.back());
  Current = newBlock();
}

void CfgBuilder::walkReturn(int s) {
  const Node &returnStmt = getNode(s);
  Terminator t{};
  t.Kind = TermKind::Return;
  if (!returnStmt.children.empty())
    t.RetVal = walkExpr(returnStmt.children.front());
  CfgModel.Blocks[Current].Term = t;
  Current = newBlock();
}

void CfgBuilder::walkLabel(int s) {
  const Node &labelNode = getNode(s);

  BB &cur = CfgModel.Blocks[Current];
  if (!cur.Statements.empty() || cur.Term.Kind != TermKind::Fallthrough) {
    int labelBB = newBlock();
    sealTo(labelBB);
    Current = labelBB;
  }

  LabelBlocks[labelNode.detail] = Current;
  for (int childIdx : labelNode.children)
    walkStmt(childIdx);
}

void CfgBuilder::walkGoto(int s) {
  const Node &gotoNode = getNode(s);
  Terminator t{};
  t.Kind = TermKind::Jump;
  t.Target = -1;
  CfgModel.Blocks[Current].Term = t;
  PendingGotos.push_back({Current, gotoNode.detail});
  Current = newBlock();
}

void CfgBuilder::walkSwitch(int s) {
  const Node &swNode = getNode(s);
  assert(swNode.children.size() == 3);

  int initI = swNode.children[0];
  int condI = swNode.children[1];
  int bodyI = swNode.children[2];

  bool hasInit = getNode(initI).kind != "EmptyInit";

  if (hasInit)
    walkStmt(initI);

  const std::string val = walkExpr(condI);

  int switchBB = Current;

  Terminator t{};
  t.Kind = TermKind::Switch;
  t.SwitchVal = val;
  t.DefaultTarget = -1;
  CfgModel.Blocks[switchBB].Term = t;

  int joinBB = newBlock();

  Current = newBlock();

  BreakTargets.push_back(joinBB);
  SwitchTargets.push_back(switchBB);

  walkStmt(bodyI);
  sealTo(joinBB);

  SwitchTargets.pop_back();
  BreakTargets.pop_back();

  Terminator &finalTerm = CfgModel.Blocks[switchBB].Term;
  if (finalTerm.DefaultTarget < 0)
    finalTerm.DefaultTarget = joinBB;

  Current = joinBB;
}

void CfgBuilder::walkCase(int s) {
  const Node &caseNode = getNode(s);

  assert(!SwitchTargets.empty());
  int switchBB = SwitchTargets.back();

  BB &cur = CfgModel.Blocks[Current];
  if (!cur.Statements.empty() || cur.Term.Kind != TermKind::Fallthrough) {
    int caseBB = newBlock();
    sealTo(caseBB);
    Current = caseBB;
  }

  long long value = 0;
  bool parsed = false;
  if (!caseNode.detail.empty() && caseNode.detail != "?") {
    char *end = nullptr;
    value = std::strtoll(caseNode.detail.c_str(), &end, 10);
    parsed = (end != caseNode.detail.c_str()) && (*end == '\0');
  }

  if (parsed)
    CfgModel.Blocks[switchBB].Term.Cases.push_back({value, Current});

  for (int childIdx : caseNode.children)
    walkStmt(childIdx);
}

void CfgBuilder::walkDefault(int s) {
  const Node &defaultNode = getNode(s);

  assert(!SwitchTargets.empty());
  int switchBB = SwitchTargets.back();

  BB &cur = CfgModel.Blocks[Current];
  if (!cur.Statements.empty() || cur.Term.Kind != TermKind::Fallthrough) {
    int defaultBB = newBlock();
    sealTo(defaultBB);
    Current = defaultBB;
  }

  CfgModel.Blocks[switchBB].Term.DefaultTarget = Current;

  for (int childIdx : defaultNode.children)
    walkStmt(childIdx);
}

void CfgBuilder::patchGotos() {
  for (const auto &pg : PendingGotos) {
    auto it = LabelBlocks.find(pg.Label);
    if (it != LabelBlocks.end())
      CfgModel.Blocks[pg.Block].Term.Target = it->second;
  }
  PendingGotos.clear();
  LabelBlocks.clear();
}

void CfgBuilder::walkFunction(int f) {
  const Node &funcNode = getNode(f);

  std::string funcName = funcNode.detail;
  std::vector<std::string> funcParams;
  for (int childIdx : funcNode.children) {
    const Node &child = getNode(childIdx);
    if (child.kind != "ParmVar")
      break;
    funcParams.push_back(child.detail);
  }
  CfgModel.FuncDecl = stringifyFuncDecl(funcName, funcParams);

  int compStmtIdx = funcNode.children.back();
  if (getNode(compStmtIdx).kind != "CompoundStmt")
    return;
  CfgModel.Blocks.push_back(BB{});
  Current = static_cast<int>(CfgModel.Blocks.size() - 1);
  walkStmt(compStmtIdx);

  {
    BB &b = CfgModel.Blocks[Current];
    if (b.Term.Kind == TermKind::Fallthrough) {
      Terminator t{};
      t.Kind = TermKind::Return;
      b.Term = t;
    }
  }

  patchGotos();
}

void CfgBuilder::build(const TreeModel &ast) {
  if (ast.nodes.empty())
    return;
  Tree = &ast;
  const Node &root = getNode(0);
  bool functionFound = false;
  for (int childIdx : root.children) {
    if (getNode(childIdx).kind == "Function") {
      assert(!functionFound);
      functionFound = true;
      walkFunction(childIdx);
    }
  }
}

} // namespace cpp2model