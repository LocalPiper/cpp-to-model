#include "dotgen.h"
#include <algorithm>
#include <functional>
#include <sstream>
#include <vector>

namespace cpp2model {

std::string dot_escape(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      break;
    case '\t':
      out += "    ";
      break;
    default:
      out += c;
      break;
    }
  }
  return out;
}

namespace {

std::string terminator_text(const Terminator &t) {
  switch (t.kind) {
  case TermKind::Fallthrough:
    return "";
  case TermKind::Jump:
    return "jump BB" + std::to_string(t.target);
  case TermKind::Branch:
    return "if " + t.cond + " ? BB" + std::to_string(t.on_true) + " : BB" +
           std::to_string(t.on_false);
  case TermKind::Switch:
    return "switch " + t.switch_val;
  case TermKind::Return:
    return t.ret_val.empty() ? "return" : "return " + t.ret_val;
  }
  return "";
}

std::vector<std::pair<int, std::string>> block_successors(const BB &bb) {
  std::vector<std::pair<int, std::string>> out;
  switch (bb.term.kind) {
  case TermKind::Fallthrough:
    break;
  case TermKind::Jump:
    if (bb.term.target >= 0)
      out.emplace_back(bb.term.target, "");
    break;
  case TermKind::Branch:
    if (bb.term.on_true >= 0)
      out.emplace_back(bb.term.on_true, "true");
    if (bb.term.on_false >= 0)
      out.emplace_back(bb.term.on_false, "false");
    break;
  case TermKind::Switch:
    for (auto &[v, t] : bb.term.cases)
      if (t >= 0)
        out.emplace_back(t, std::to_string(v));
    if (bb.term.default_target >= 0)
      out.emplace_back(bb.term.default_target, "default");
    break;
  case TermKind::Return:
    break;
  }
  return out;
}

bool is_conditional_edge(const BB &bb) {
  return bb.term.kind == TermKind::Branch || bb.term.kind == TermKind::Switch;
}

std::vector<char> reachable_blocks(const Cfg &cfg) {
  const int n = static_cast<int>(cfg.blocks.size());
  std::vector<char> seen(n, 0);
  if (n == 0)
    return seen;
  std::vector<int> stack = {0};
  seen[0] = 1;
  while (!stack.empty()) {
    int u = stack.back();
    stack.pop_back();
    for (auto &[v, _] : block_successors(cfg.blocks[u])) {
      if (v < 0 || v >= n)
        continue;
      if (!seen[v]) {
        seen[v] = 1;
        stack.push_back(v);
      }
    }
  }
  return seen;
}

std::vector<EdgeKind>
classify_edges(const Cfg &cfg, const std::vector<std::pair<int, int>> &edges) {
  const int n = static_cast<int>(cfg.blocks.size());
  std::vector<std::vector<int>> out_edges(n);
  for (size_t ei = 0; ei < edges.size(); ++ei) {
    int u = edges[ei].first;
    if (u >= 0 && u < n)
      out_edges[u].push_back(static_cast<int>(ei));
  }

  enum { WHITE, GRAY, BLACK };
  std::vector<char> color(n, WHITE);
  std::vector<char> is_back(edges.size(), 0);

  std::function<void(int)> dfs = [&](int u) {
    color[u] = GRAY;
    for (int ei : out_edges[u]) {
      int v = edges[ei].second;
      if (v < 0 || v >= n)
        continue;
      if (color[v] == GRAY)
        is_back[ei] = 1;
      else if (color[v] == WHITE)
        dfs(v);
    }
    color[u] = BLACK;
  };
  for (int i = 0; i < n; ++i)
    if (color[i] == WHITE)
      dfs(i);

  std::vector<int> depth(n, 0);
  for (int iter = 0; iter < n; ++iter) {
    bool changed = false;
    for (size_t ei = 0; ei < edges.size(); ++ei) {
      if (is_back[ei])
        continue;
      int u = edges[ei].first, v = edges[ei].second;
      if (u < 0 || v < 0 || u >= n || v >= n)
        continue;
      if (depth[v] < depth[u] + 1) {
        depth[v] = depth[u] + 1;
        changed = true;
      }
    }
    if (!changed)
      break;
  }

  std::vector<EdgeKind> kinds(edges.size());
  for (size_t ei = 0; ei < edges.size(); ++ei) {
    if (is_back[ei]) {
      kinds[ei] = EdgeKind::Back;
    } else {
      int u = edges[ei].first, v = edges[ei].second;
      kinds[ei] = (v >= 0 && v < n && depth[v] > depth[u]) ? EdgeKind::Forward
                                                           : EdgeKind::Cross;
    }
  }
  return kinds;
}

std::string ssa_tac_text(const TacOp &op) {
  std::string out = op.dst + " = ";
  if (op.op == "=") {
    out += op.args.empty() ? "" : op.args[0];
  } else if (op.args.size() == 1) {
    const bool spaced = (op.op == "sizeof" || op.op == "alignof");
    out += op.op;
    if (spaced)
      out += " ";
    out += op.args[0];
  } else if (op.args.size() == 2) {
    out += op.args[0] + " " + op.op + " " + op.args[1];
  } else {
    out += op.op;
  }
  return out;
}

std::string ssa_phi_text(const PhiNode &p) {
  std::string out = p.var + " = φ(";
  for (size_t i = 0; i < p.incoming.size(); ++i) {
    if (i)
      out += ", ";
    out += p.incoming[i].value;
    out += " [BB" + std::to_string(p.incoming[i].pred_block) + "]";
  }
  out += ")";
  return out;
}

struct SsaEdge {
  int from;
  int to;
  std::string label;
  bool conditional;
};

std::vector<SsaEdge> ssa_successor_edges(const SsaCfg &ssa) {
  std::vector<SsaEdge> out;
  const int n = static_cast<int>(ssa.blocks.size());
  for (int u = 0; u < n; ++u) {
    const Terminator &t = ssa.blocks[u].term;
    switch (t.kind) {
    case TermKind::Fallthrough:
      break;
    case TermKind::Jump:
      if (t.target >= 0)
        out.push_back({u, t.target, "", false});
      break;
    case TermKind::Branch:
      if (t.on_true >= 0)
        out.push_back({u, t.on_true, "true", true});
      if (t.on_false >= 0)
        out.push_back({u, t.on_false, "false", true});
      break;
    case TermKind::Switch:
      for (auto &[v, tgt] : t.cases)
        if (tgt >= 0)
          out.push_back({u, tgt, std::to_string(v), true});
      if (t.default_target >= 0)
        out.push_back({u, t.default_target, "default", true});
      break;
    case TermKind::Return:
      break;
    }
  }
  return out;
}

std::vector<char> ssa_reachable(const SsaCfg &ssa) {
  const int n = static_cast<int>(ssa.blocks.size());
  std::vector<char> seen(n, 0);
  if (n == 0)
    return seen;
  std::vector<int> stack = {0};
  seen[0] = 1;
  while (!stack.empty()) {
    int u = stack.back();
    stack.pop_back();
    for (int v : ssa.blocks[u].succs) {
      if (v < 0 || v >= n)
        continue;
      if (!seen[v]) {
        seen[v] = 1;
        stack.push_back(v);
      }
    }
  }
  return seen;
}

std::vector<EdgeKind> ssa_classify_edges(const SsaCfg &ssa,
                                         const std::vector<SsaEdge> &edges) {
  const int n = static_cast<int>(ssa.blocks.size());
  std::vector<std::vector<int>> out_edges(n);
  for (size_t ei = 0; ei < edges.size(); ++ei) {
    int u = edges[ei].from;
    if (u >= 0 && u < n)
      out_edges[u].push_back(static_cast<int>(ei));
  }

  enum { WHITE, GRAY, BLACK };
  std::vector<char> color(n, WHITE);
  std::vector<char> is_back(edges.size(), 0);

  std::function<void(int)> dfs = [&](int u) {
    color[u] = GRAY;
    for (int ei : out_edges[u]) {
      int v = edges[ei].to;
      if (v < 0 || v >= n)
        continue;
      if (color[v] == GRAY)
        is_back[ei] = 1;
      else if (color[v] == WHITE)
        dfs(v);
    }
    color[u] = BLACK;
  };
  for (int i = 0; i < n; ++i)
    if (color[i] == WHITE)
      dfs(i);

  std::vector<int> depth(n, 0);
  for (int iter = 0; iter < n; ++iter) {
    bool changed = false;
    for (size_t ei = 0; ei < edges.size(); ++ei) {
      if (is_back[ei])
        continue;
      int u = edges[ei].from, v = edges[ei].to;
      if (u < 0 || v < 0 || u >= n || v >= n)
        continue;
      if (depth[v] < depth[u] + 1) {
        depth[v] = depth[u] + 1;
        changed = true;
      }
    }
    if (!changed)
      break;
  }

  std::vector<EdgeKind> kinds(edges.size());
  for (size_t ei = 0; ei < edges.size(); ++ei) {
    if (is_back[ei]) {
      kinds[ei] = EdgeKind::Back;
    } else {
      int u = edges[ei].from, v = edges[ei].to;
      kinds[ei] = (v >= 0 && v < n && depth[v] > depth[u]) ? EdgeKind::Forward
                                                           : EdgeKind::Cross;
    }
  }
  return kinds;
}

CfgColors ssa_block_colors(const SsaCfg &ssa, int i) {
  const SsaBB &bb = ssa.blocks[i];
  CfgColors c;
  if (i == 0) {
    c.fill = "#e3f2fd";
    c.stroke = "#1e88e5";
    c.text = "#0d47a1";
  } else if (bb.term.kind == TermKind::Return) {
    c.fill = "#e8f5e9";
    c.stroke = "#43a047";
    c.text = "#1b5e20";
  } else if (!bb.phis.empty()) {
    c.fill = "#f3e5f5";
    c.stroke = "#8e24aa";
    c.text = "#4a148c";
  } else {
    c.fill = "#fafafa";
    c.stroke = "#9e9e9e";
    c.text = "#212121";
  }
  return c;
}

} // namespace

CfgColors cfg_block_colors(const Cfg &cfg, int bb_index) {
  CfgColors c;
  const BB &bb = cfg.blocks[bb_index];

  const bool is_entry = (bb_index == 0);
  const bool is_exit = (bb.term.kind == TermKind::Return);

  if (is_entry) {
    c.fill = "#e3f2fd";
    c.stroke = "#1e88e5";
    c.text = "#0d47a1";
  } else if (is_exit) {
    c.fill = "#e8f5e9";
    c.stroke = "#43a047";
    c.text = "#1b5e20";
  } else {
    c.fill = "#fafafa";
    c.stroke = "#9e9e9e";
    c.text = "#212121";
  }
  return c;
}

CfgEdgeColors cfg_edge_colors(EdgeKind kind, bool is_conditional) {
  CfgEdgeColors c;
  switch (kind) {
  case EdgeKind::Forward:
    c.color = is_conditional ? "#546e7a" : "#78909c";
    c.fontcolor = "#546e7a";
    break;
  case EdgeKind::Back:
    c.color = "#b71c1c";
    c.fontcolor = "#b71c1c";
    break;
  case EdgeKind::Cross:
    c.color = "#ef6c00";
    c.fontcolor = "#ef6c00";
    break;
  }
  return c;
}

std::string cfg_to_dot(const Cfg &cfg) {
  std::ostringstream s;
  const int n = static_cast<int>(cfg.blocks.size());

  s << "digraph cfg {\n";
  s << "  rankdir=TB;\n";
  s << "  graph [fontname=\"monospace\"];\n";
  s << "  node [shape=box, style=\"rounded,filled\", "
       "fontname=\"monospace\", fontsize=10, margin=\"0.12,0.06\"];\n";
  s << "  edge [fontname=\"monospace\", fontsize=9, arrowsize=0.7];\n";
  s << "  ordering=out;\n";
  s << "\n";

  std::vector<char> reach = reachable_blocks(cfg);

  {
    std::string label = cfg.func_decl.empty() ? "(no function)" : cfg.func_decl;
    s << "  func [label=\"" << dot_escape(label)
      << "\", shape=box, "
         "style=\"rounded,filled\", fillcolor=\"#e3f2fd\", "
         "color=\"#1e88e5\", fontcolor=\"#0d47a1\", fontsize=11];\n";
    if (n > 0 && reach[0])
      s << "  func -> BB0;\n";
    s << "\n";
  }

  for (int i = 0; i < n; ++i) {
    if (!reach[i])
      continue;
    const BB &bb = cfg.blocks[i];
    const CfgColors col = cfg_block_colors(cfg, i);

    std::string label = "BB" + std::to_string(i);
    for (const std::string &st : bb.statements) {
      label += "\\l" + dot_escape(st);
    }
    if (bb.term.kind != TermKind::Fallthrough) {
      label += "\\l" + dot_escape(terminator_text(bb.term));
    }
    label += "\\l";

    s << "  BB" << i << " [label=\"" << label << "\""
      << ", fillcolor=\"" << col.fill << "\""
      << ", color=\"" << col.stroke << "\""
      << ", fontcolor=\"" << col.text << "\"];\n";
  }
  s << "\n";

  std::vector<std::pair<int, int>> edge_list;
  std::vector<std::string> edge_labels;
  for (int u = 0; u < n; ++u) {
    for (auto &[v, lab] : block_successors(cfg.blocks[u])) {
      if (v < 0 || v >= n)
        continue;
      edge_list.emplace_back(u, v);
      edge_labels.push_back(lab);
    }
  }

  std::vector<EdgeKind> kinds = classify_edges(cfg, edge_list);

  for (size_t ei = 0; ei < edge_list.size(); ++ei) {
    int u = edge_list[ei].first;
    int v = edge_list[ei].second;
    if (!reach[u] || !reach[v])
      continue;

    const EdgeKind kind = kinds[ei];
    const bool cond = is_conditional_edge(cfg.blocks[u]);
    const CfgEdgeColors ec = cfg_edge_colors(kind, cond);

    s << "  BB" << u << " -> BB" << v << " [";

    std::string lab = edge_labels[ei];
    if (lab.empty() && kind == EdgeKind::Back)
      lab = "back";
    if (!lab.empty()) {
      s << "label=\"" << dot_escape(lab) << "\", ";
    }

    s << "color=\"" << ec.color << "\"";
    if (!lab.empty())
      s << ", fontcolor=\"" << ec.fontcolor << "\"";

    if (kind == EdgeKind::Back) {
      s << ", constraint=false, style=dashed";
    }

    s << "];\n";
  }

  s << "}\n";
  return s.str();
}

std::string ast_to_dot(const TreeModel &tree) {
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
  s << "  ordering=out;\n";
  s << "\n";

  auto fill_for = [](NodeCategory c) -> const char * {
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
  };
  auto stroke_for = [](NodeCategory c) -> const char * {
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
  };
  auto text_for = [](NodeCategory c) -> const char * {
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
  };

  for (size_t i = 0; i < n; ++i) {
    const Node &nd = tree.nodes[i];

    std::string label = dot_escape(nd.kind);
    if (!nd.detail.empty())
      label += "\\n" + dot_escape(nd.detail);
    if (!nd.type.empty())
      label += "\\n" + dot_escape(nd.type);
    if (!nd.range.empty())
      label += "\\n" + dot_escape(nd.range);

    std::string tooltip = nd.kind;
    if (!nd.detail.empty())
      tooltip += " | " + nd.detail;
    if (!nd.type.empty())
      tooltip += " | " + nd.type;
    if (!nd.range.empty())
      tooltip += " | " + nd.range;

    s << "  n" << i << " [label=\"" << label << "\""
      << ", tooltip=\"" << dot_escape(tooltip) << "\""
      << ", fillcolor=\"" << fill_for(nd.cat) << "\""
      << ", color=\"" << stroke_for(nd.cat) << "\""
      << ", fontcolor=\"" << text_for(nd.cat) << "\"";
    if (nd.cat == NodeCategory::Root)
      s << ", fontsize=11";
    s << "];\n";
  }

  for (size_t i = 0; i < n; ++i) {
    for (int c : tree.nodes[i].children) {
      if (c < 0 || static_cast<size_t>(c) >= n)
        continue;
      s << "  n" << i << " -> n" << c << ";\n";
    }
  }

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

std::string ssa_to_dot(const SsaCfg &ssa) {
  std::ostringstream s;
  const int n = static_cast<int>(ssa.blocks.size());

  s << "digraph ssa {\n";
  s << "  rankdir=TB;\n";
  s << "  graph [fontname=\"monospace\"];\n";
  s << "  node [shape=box, style=\"rounded,filled\", "
       "fontname=\"monospace\", fontsize=10, margin=\"0.12,0.06\"];\n";
  s << "  edge [fontname=\"monospace\", fontsize=9, arrowsize=0.7];\n";
  s << "  ordering=out;\n";
  s << "\n";

  std::vector<char> reach = ssa_reachable(ssa);

  {
    std::string label = ssa.func_decl.empty() ? "(no function)" : ssa.func_decl;
    s << "  func [label=\"" << dot_escape(label)
      << "\", shape=box, "
         "style=\"rounded,filled\", fillcolor=\"#e3f2fd\", "
         "color=\"#1e88e5\", fontcolor=\"#0d47a1\", fontsize=11];\n";
    if (n > 0 && reach[0])
      s << "  func -> BB0;\n";
    s << "\n";
  }

  for (int i = 0; i < n; ++i) {
    if (!reach[i])
      continue;
    const SsaBB &bb = ssa.blocks[i];
    const CfgColors col = ssa_block_colors(ssa, i);

    std::string label = "BB" + std::to_string(i);
    if (!bb.preds.empty()) {
      label += " [preds:";
      for (size_t k = 0; k < bb.preds.size(); ++k) {
        label += " BB" + std::to_string(bb.preds[k]);
      }
      label += "]";
    }

    for (const PhiNode &p : bb.phis) {
      label += "\\l" + dot_escape(ssa_phi_text(p));
    }
    for (const TacOp &op : bb.ops) {
      label += "\\l" + dot_escape(ssa_tac_text(op));
    }
    if (bb.term.kind != TermKind::Fallthrough) {
      label += "\\l" + dot_escape(terminator_text(bb.term));
    }
    label += "\\l";

    s << "  BB" << i << " [label=\"" << label << "\""
      << ", fillcolor=\"" << col.fill << "\""
      << ", color=\"" << col.stroke << "\""
      << ", fontcolor=\"" << col.text << "\"];\n";
  }
  s << "\n";

  std::vector<SsaEdge> edges = ssa_successor_edges(ssa);
  std::vector<EdgeKind> kinds = ssa_classify_edges(ssa, edges);

  for (size_t ei = 0; ei < edges.size(); ++ei) {
    const SsaEdge &e = edges[ei];
    if (e.from < 0 || e.from >= n)
      continue;
    if (e.to < 0 || e.to >= n)
      continue;
    if (!reach[e.from] || !reach[e.to])
      continue;

    const EdgeKind kind = kinds[ei];
    const CfgEdgeColors ec = cfg_edge_colors(kind, e.conditional);

    s << "  BB" << e.from << " -> BB" << e.to << " [";

    std::string lab = e.label;
    if (lab.empty() && kind == EdgeKind::Back)
      lab = "back";
    if (!lab.empty()) {
      s << "label=\"" << dot_escape(lab) << "\", ";
    }

    s << "color=\"" << ec.color << "\"";
    if (!lab.empty())
      s << ", fontcolor=\"" << ec.fontcolor << "\"";

    if (kind == EdgeKind::Back) {
      s << ", constraint=false, style=dashed";
    }

    s << "];\n";
  }

  s << "}\n";
  return s.str();
}

} // namespace cpp2model