#pragma once
#include "../model.h"
#include <vector>
#include <string>
#include <variant>

namespace cpp2model {
enum class TermKind {Fallthrough, Jump, Branch, Switch, Return };

struct Terminator {
    TermKind kind = TermKind::Fallthrough;
    // branch
    std::string cond;
    int on_true = -1;
    int on_false = -1;
    // jump
    int target = -1;
    // switch
    std::string switch_val;
    std::vector<std::pair<long long, int>> cases;
    int default_target = -1;
    // return
    std::string ret_val;

};

struct BB {
    std::vector<std::string> statements;
    Terminator term;
};

struct Cfg {
    std::string func_decl;
    std::vector<BB> blocks;
};

class CfgBuilder {
public:
    void build(const TreeModel& ast);
    const Cfg& cfg() const { return m_cfg; }

private:
    Cfg m_cfg;
    TreeModel m_tree;
    int m_current = -1;
    int m_temp_cnt = 0;
    std::vector<int> m_break_targets;
    std::vector<int> m_continue_targets;

    void walkFunction(int f);
    void walkStmt(int s);
    void walkIf(int s);
    void walkWhile(int s);
    void walkFor(int s);
    void walkDoWhile(int s);
    void walkBreak();
    void walkContinue();
    void walkReturn(int s);

    std::string walkExpr(int e, bool temp_needed, bool value_needed);
    std::string walkUnary(int e, bool temp_needed, bool value_needed);
    std::string walkBinary(int e, bool temp_needed, bool value_needed);

    bool isValueNode(const Node& n) const;
    bool isCompoundAssign(const std::string& op) const;
    std::string compoundBase(const std::string& op) const;
    bool isPostfixUnary(const Node& n) const;
    bool isSpecialUnary(const std::string& op) const;

    const Node& getNode(int i) const;
    void addToBlock(const std::string& stmt);
    int newBlock();
    int branch(const std::string& cond, int on_true, int on_false);
    void sealTo(int target);
    std::string makeTemp();
};

} // namespace cpp2model