#pragma once

#include <string>
#include <vector>

namespace cpp2model {

enum class NodeCategory {
    Root = 0,
    Declaration = 1,
    Statement = 2,
    Expression = 3,
};

struct Node {
    NodeCategory cat = NodeCategory::Statement;
    std::string kind;
    std::string detail;
    std::string type;
    std::string range;
    std::vector<int> children;
};

struct TreeModel {
    std::vector<Node> nodes;
    bool truncated = false;
};

}  // namespace cpp2model