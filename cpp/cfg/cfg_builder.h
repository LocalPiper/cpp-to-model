#pragma once
#include "../model.h"
#include <vector>
#include <string>
#include <variant>

namespace cpp2model {

struct BB {
    std::vector<std::string> statements;
};


struct Condition {
    std::string cond;
    int on_true;
    int on_false;
};


using CfgComponent = std::variant<BB, Condition>;

struct Cfg {
    std::string func_decl;
    std::vector<CfgComponent> blocks;
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
    void walkFunction(int f);
    void walkStmt(int s);

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
    std::string makeTemp();
};

} // namespace cpp2model