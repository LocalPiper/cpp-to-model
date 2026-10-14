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
  int pred_block = -1;
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
  std::string func_decl;
  std::vector<SsaBB> blocks;
  std::vector<int> rpo_num;
  std::vector<int> idom;
  std::vector<std::vector<int>> df;
};

class SsaBuilder {
public:
  void build(const Cfg &cfg);
  const SsaCfg &ssa() const { return m_ssa; }

private:
  SsaCfg m_ssa;
  std::vector<int> m_rpo;
  std::vector<int> m_rpo_num;
  std::vector<int> m_idom;
  std::vector<std::vector<int>> m_df;
  std::unordered_map<std::string, std::vector<int>> m_var_stack;
  std::unordered_map<std::string, int> m_var_counter;
  std::vector<std::vector<int>> m_dom_children;
  std::unordered_set<std::string> m_defined_names;

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

  static TacOp parse_tac(const std::string &stmt);
  static ExprFragment parse_expr_fragment(const std::string &expr);
  std::string rename_use(const std::string &name);
  std::string rename_def(const std::string &name);
  bool is_ssa_renameable(const std::string &name);

  static bool is_name_token(const std::string &tok);
  static bool is_temp(const std::string &name);
  static bool is_unary_op(const std::string &tok);
  static bool is_binary_op(const std::string &tok);
  static bool is_special_unary(const std::string &tok);
};

} // namespace cpp2model