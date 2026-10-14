#include "ssa_dot.h"

#include <sstream>

#include "dot_escape.h"
#include "ssa_text.h"

namespace cpp2model {

CfgColors ssaBlockColors(const SsaCfg &ssa, int bbIndex) {
  const SsaBB &bb = ssa.blocks[bbIndex];
  CfgColors c;
  if (bbIndex == 0) {
    c.fill = "#e3f2fd";
    c.stroke = "#1e88e5";
    c.text = "#0d47a1";
  } else if (bb.term.Kind == TermKind::Return) {
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

namespace {

std::vector<std::vector<int>> ssaSuccs(const SsaCfg &ssa) {
  std::vector<std::vector<int>> succs(ssa.blocks.size());
  for (size_t i = 0; i < ssa.blocks.size(); ++i)
    succs[i] = ssa.blocks[i].succs;
  return succs;
}

} // namespace

std::string ssaToDot(const SsaCfg &ssa) {
  std::ostringstream s;
  const int n = static_cast<int>(ssa.blocks.size());

  s << "digraph ssa {\n";
  s << "  rankdir=TB;\n";
  s << "  graph [fontname=\"monospace\"];\n";
  s << "  node [shape=box, style=\"rounded,filled\", "
       "fontname=\"monospace\", fontsize=10, margin=\"0.12,0.06\"];\n";
  s << "  edge [fontname=\"monospace\", fontsize=9, arrowsize=0.7];\n";
  s << "  ordering=out;\n\n";

  const std::vector<char> reach = reachableBlocks(n, ssaSuccs(ssa));

  {
    const std::string label =
        ssa.func_decl.empty() ? "(no function)" : ssa.func_decl;
    s << "  func [label=\"" << dotEscape(label)
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
    const CfgColors col = ssaBlockColors(ssa, i);

    std::string label = "BB" + std::to_string(i);
    if (!bb.preds.empty()) {
      label += " [preds:";
      for (int p : bb.preds)
        label += " BB" + std::to_string(p);
      label += "]";
    }

    for (const PhiNode &p : bb.phis)
      label += "\\l" + dotEscape(ssaPhiText(p));
    for (const TacOp &op : bb.ops)
      label += "\\l" + dotEscape(ssaTacText(op));
    if (bb.term.Kind != TermKind::Fallthrough)
      label += "\\l" + dotEscape(terminatorText(bb.term));
    label += "\\l";

    s << "  BB" << i << " [label=\"" << label << "\""
      << ", fillcolor=\"" << col.fill << "\""
      << ", color=\"" << col.stroke << "\""
      << ", fontcolor=\"" << col.text << "\"];\n";
  }
  s << "\n";

  std::vector<DotEdge> edges;
  for (int u = 0; u < n; ++u)
    for (DotEdge e : terminatorEdges(u, ssa.blocks[u].term))
      edges.push_back(std::move(e));

  const std::vector<EdgeKind> kinds = classifyEdges(n, edges);

  for (size_t ei = 0; ei < edges.size(); ++ei) {
    const DotEdge &e = edges[ei];
    if (e.from < 0 || e.from >= n || e.to < 0 || e.to >= n)
      continue;
    if (!reach[e.from] || !reach[e.to])
      continue;

    const EdgeKind kind = kinds[ei];
    const CfgEdgeColors ec = cfgEdgeColors(kind, e.conditional);

    s << "  BB" << e.from << " -> BB" << e.to << " [";

    std::string lab = e.label;
    if (lab.empty() && kind == EdgeKind::Back)
      lab = "back";
    if (!lab.empty())
      s << "label=\"" << dotEscape(lab) << "\", ";

    s << "color=\"" << ec.color << "\"";
    if (!lab.empty())
      s << ", fontcolor=\"" << ec.fontcolor << "\"";

    if (kind == EdgeKind::Back)
      s << ", constraint=false, style=dashed";

    s << "];\n";
  }

  s << "}\n";
  return s.str();
}

} // namespace cpp2model