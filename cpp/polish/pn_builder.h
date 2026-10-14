#pragma once
#include "../model.h"
#include <string>

namespace cpp2model {

class PolishBuilder {
public:
    void build(const TreeModel& ast);
    const std::string& polish() const { return m_out; }

private:
    const TreeModel* m_tree = nullptr;
    std::string m_out;

    void walk(int idx, std::string& out);
    std::string tokenFor(const Node& n) const;
    bool isVariableArity(const std::string& kind) const;
};

}  // namespace cpp2model