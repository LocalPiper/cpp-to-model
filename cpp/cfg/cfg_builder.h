#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "../model.h"

namespace cpp2model {

enum class TermKind { Fallthrough, Jump, Branch, Switch, Return };

struct Terminator {
  TermKind Kind = TermKind::Fallthrough;
  // Branch
  std::string Cond;
  int OnTrue = -1;
  int OnFalse = -1;
  // Jump
  int Target = -1;
  // Switch
  std::string SwitchVal;
  std::vector<std::pair<long long, int>> Cases;
  int DefaultTarget = -1;
  // Return
  std::string RetVal;
};

struct BB {
  std::vector<std::string> Statements;
  Terminator Term;
};

struct Cfg {
  std::string FuncDecl;
  std::vector<BB> Blocks;
};

class CfgBuilder {
public:
  void build(const TreeModel &ast);
  const Cfg &cfg() const { return CfgModel; }

private:
  struct PendingGoto {
    int Block;
    std::string Label;
  };

  Cfg CfgModel;
  const TreeModel *Tree = nullptr;
  int Current = -1;
  int TempCount = 0;
  std::vector<int> BreakTargets;
  std::vector<int> ContinueTargets;
  std::vector<int> SwitchTargets;

  std::unordered_map<std::string, int> LabelBlocks;
  std::vector<PendingGoto> PendingGotos;

  void walkFunction(int f);
  void walkStmt(int s);
  void walkIf(int s);
  void walkWhile(int s);
  void walkFor(int s);
  void walkForEach(int s);
  void walkDoWhile(int s);
  void walkBreak();
  void walkContinue();
  void walkReturn(int s);
  void walkLabel(int s);
  void walkGoto(int s);
  void walkSwitch(int s);
  void walkCase(int s);
  void walkDefault(int s);
  void patchGotos();

  std::string walkExpr(int e, bool tempNeeded = false, bool valueNeeded = true);
  std::string walkUnary(int e, bool tempNeeded, bool valueNeeded);
  std::string walkBinary(int e, bool tempNeeded, bool valueNeeded);
  std::string walkCall(int e, bool tempNeeded, bool valueNeeded);

  bool isValueNode(const Node &n) const;
  bool isCompoundAssign(const std::string &op) const;
  std::string compoundBase(const std::string &op) const;
  bool isPostfixUnary(const Node &n) const;
  bool isSpecialUnary(const std::string &op) const;

  const Node &getNode(int i) const;
  void addToBlock(const std::string &stmt);
  int newBlock();
  void branch(const std::string &cond, int onTrue, int onFalse);
  void sealTo(int target);
  std::string makeTemp();
};

} // namespace cpp2model