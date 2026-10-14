#pragma once

#include "../cfg/cfg_builder.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace cpp2model {

struct TacOp {
  std::string dst;
  std::string op;
  std::vector<std::string> args;
};

struct ExprFragment {
  std::string op;
  std::vector<std::string> args;
};

struct PhiInput {
  std::string value;
  int predBlock = -1;
};

struct PhiNode {
  std::string var;
  std::string base;
  std::vector<PhiInput> incoming;
};

struct SsaBB {
  std::vector<PhiNode> phis;
  std::vector<TacOp> ops;
  Terminator term;
  std::vector<int> preds;
  std::vector<int> succs;
};

struct SsaCfg {
  std::string funcDecl;
  std::vector<SsaBB> blocks;
  std::vector<int> rpoNum;
  std::vector<int> idom;
  std::vector<std::vector<int>> df;
};

class SsaBuilder {
public:
  void build(const Cfg &cfg);
  const SsaCfg &ssa() const { return Ssa; }

private:
  SsaCfg Ssa;
  std::vector<int> Rpo;
  std::vector<int> RpoNum;
  std::vector<int> Idoms;
  std::vector<std::vector<int>> Df;
  std::unordered_map<std::string, std::vector<int>> VarStack;
  std::unordered_map<std::string, int> VarCounter;
  std::vector<std::vector<int>> DomChildren;
  std::unordered_set<std::string> DefinedNames;

  void computeRpo();
  void computeDominators();
  int intersect(int a, int b);
  void computeDominanceFrontiers();
  void placePhis();

  void computeEdges();
  void parseBlocks(const Cfg &cfg);

  void rename();
  void renameBlock(int b);
  void renameOp(TacOp &op);
  void renameTerminator(Terminator &t);
  void renameFragment(std::string &field);
  void fillPhiInputs(int b);
  void buildDomChildren();
  void collectDefinedNames();

  static TacOp parseTac(const std::string &stmt);
  static ExprFragment parseExprFragment(const std::string &expr);
  std::string renameUse(const std::string &name);
  std::string renameDef(const std::string &name);
  bool isSsaRenameable(const std::string &name);

  static bool isNameToken(const std::string &tok);
  static bool isTemp(const std::string &name);
  static bool isUnaryOp(const std::string &tok);
  static bool isBinaryOp(const std::string &tok);
  static bool isSpecialUnary(const std::string &tok);
};

} // namespace cpp2model