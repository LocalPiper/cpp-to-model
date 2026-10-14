#pragma once

#include "../cfg/cfg_builder.h"

#include <string>
#include <vector>
#include <unordered_map>

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
    void build(const Cfg& cfg);
    const SsaCfg& ssa() const { return m_ssa; }

private:
    SsaCfg m_ssa;
    std::vector<int> m_rpo;
    std::vector<int> m_rpo_num;
    std::vector<int> m_idom;
    std::vector<std::vector<int>> m_df;

    void computeRpo();
    void computeDominators();
    int intersect(int a, int b);
    void computeDominanceFrontiers();

    void computeEdges();
    void parseBlocks(const Cfg& cfg);

    static TacOp        parse_tac(const std::string& stmt);
    static ExprFragment parse_expr_fragment(const std::string& expr);

    static bool is_name_token(const std::string& tok);
    static bool is_temp(const std::string& name);
    static bool is_unary_op(const std::string& tok);
    static bool is_binary_op(const std::string& tok);
    static bool is_special_unary(const std::string& tok);
};

} // namespace cpp2model