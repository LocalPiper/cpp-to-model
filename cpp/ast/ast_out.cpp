#include "ast_out.h"

#include <ostream>
#include <vector>

#include "../text_utils.h"

namespace cpp2model {
namespace {

void emitNodeJson(std::ostream &os, const std::vector<Node> &nodes, int i) {
  const Node &n = nodes[i];
  os << "{";
  os << "\"kind\":\"" << escapeJson(n.kind) << "\"";
  if (!n.detail.empty())
    os << ",\"detail\":\"" << escapeJson(n.detail) << "\"";
  if (!n.type.empty())
    os << ",\"type\":\"" << escapeJson(n.type) << "\"";
  if (!n.range.empty())
    os << ",\"range\":\"" << escapeJson(n.range) << "\"";
  if (!n.children.empty()) {
    os << ",\"children\":[";
    for (size_t k = 0; k < n.children.size(); ++k) {
      if (k)
        os << ",";
      emitNodeJson(os, nodes, n.children[k]);
    }
    os << "]";
  }
  os << "}";
}

} // namespace

void emitTreeJson(std::ostream &os, const TreeModel &tree) {
  if (tree.nodes.empty()) {
    os << "null";
    return;
  }
  emitNodeJson(os, tree.nodes, 0);
}

} // namespace cpp2model