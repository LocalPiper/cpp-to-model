#include "ast_dot.h"

#include <sstream>

#include "dot_escape.h"

namespace cpp2model {
namespace {

const char *fillFor(NodeCategory c) {
  switch (c) {
  case NodeCategory::Root:
    return "#263238";
  case NodeCategory::Declaration:
    return "#e3f2fd";
  case NodeCategory::Statement:
    return "#e8f5e9";
  case NodeCategory::Expression:
    return "#fff8e1";
  }
  return "#eceff1";
}

const char *strokeFor(NodeCategory c) {
  switch (c) {
  case NodeCategory::Root:
    return "#37474f";
  case NodeCategory::Declaration:
    return "#1e88e5";
  case NodeCategory::Statement:
    return "#43a047";
  case NodeCategory::Expression:
    return "#f9a825";
  }
  return "#78909c";
}

const char *textFor(NodeCategory c) {
  switch (c) {
  case NodeCategory::Root:
    return "#ffffff";
  case NodeCategory::Declaration:
    return "#0d47a1";
  case NodeCategory::Statement:
    return "#1b5e20";
  case NodeCategory::Expression:
    return "#5d4037";
  }
  return "#37474f";
}

} // namespace

std::string astToDot(const TreeModel &tree) {
  std::ostringstream s;
  const size_t n = tree.nodes.size();
  if (n == 0)
    return "digraph ast {}\n";

  s << "digraph ast {\n";
  s << "  rankdir=TB;\n";
  s << "  graph [fontname=\"monospace\"];\n";
  s << "  node [shape=box, style=\"rounded,filled\", "
       "fontname=\"monospace\", fontsize=10, margin=\"0.10,0.05\"];\n";
  s << "  edge [color=\"#90a4ae\", arrowsize=0.6];\n";
  s << "  ordering=out;\n\n";

  for (size_t i = 0; i < n; ++i) {
    const Node &nd = tree.nodes[i];

    std::string label = dotEscape(nd.kind);
    if (!nd.detail.empty())
      label += "\\n" + dotEscape(nd.detail);
    if (!nd.type.empty())
      label += "\\n" + dotEscape(nd.type);
    if (!nd.range.empty())
      label += "\\n" + dotEscape(nd.range);

    std::string tooltip = nd.kind;
    if (!nd.detail.empty())
      tooltip += " | " + nd.detail;
    if (!nd.type.empty())
      tooltip += " | " + nd.type;
    if (!nd.range.empty())
      tooltip += " | " + nd.range;

    s << "  n" << i << " [label=\"" << label << "\""
      << ", tooltip=\"" << dotEscape(tooltip) << "\""
      << ", fillcolor=\"" << fillFor(nd.cat) << "\""
      << ", color=\"" << strokeFor(nd.cat) << "\""
      << ", fontcolor=\"" << textFor(nd.cat) << "\"";
    if (nd.cat == NodeCategory::Root)
      s << ", fontsize=11";
    s << "];\n";
  }

  for (size_t i = 0; i < n; ++i)
    for (int c : tree.nodes[i].children)
      if (c >= 0 && static_cast<size_t>(c) < n)
        s << "  n" << i << " -> n" << c << ";\n";

  if (tree.truncated) {
    s << "  trunc [label=\"... AST truncated\", shape=note, "
         "fillcolor=\"#ffebee\", color=\"#b71c1c\", "
         "fontcolor=\"#b71c1c\", fontsize=10];\n";
    if (n > 0)
      s << "  n0 -> trunc [style=dotted, color=\"#b71c1c\"];\n";
  }

  s << "}\n";
  return s.str();
}

} // namespace cpp2model