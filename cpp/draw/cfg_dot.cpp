#include "cfg_dot.h"

#include <sstream>

#include "dot_escape.h"

namespace cpp2model {

CfgColors cfgBlockColors(const Cfg &cfg, int bbIndex) {
  CfgColors c;
  const BB &bb = cfg.Blocks[bbIndex];

  const bool isEntry = (bbIndex == 0);
  const bool isExit = (bb.Term.Kind == TermKind::Return);

  if (isEntry) {
    c.fill = "#e3f2fd";
    c.stroke = "#1e88e5";
    c.text = "#0d47a1";
  } else if (isExit) {
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

namespace {

std::vector<std::vector<int>> cfgSuccs(const Cfg &cfg) {
  std::vector<std::vector<int>> succs(cfg.Blocks.size());
  for (size_t i = 0; i < cfg.Blocks.size(); ++i) {
    for (const DotEdge &e :
         terminatorEdges(static_cast<int>(i), cfg.Blocks[i].Term))
      succs[i].push_back(e.to);
  }
  return succs;
}

} // namespace

std::string cfgToDot(const Cfg &cfg) {
  std::ostringstream s;
  const int n = static_cast<int>(cfg.Blocks.size());

  s << "digraph cfg {\n";
  s << "  rankdir=TB;\n";
  s << "  graph [fontname=\"monospace\"];\n";
  s << "  node [shape=box, style=\"rounded,filled\", "
       "fontname=\"monospace\", fontsize=10, margin=\"0.12,0.06\"];\n";
  s << "  edge [fontname=\"monospace\", fontsize=9, arrowsize=0.7];\n";
  s << "  ordering=out;\n\n";

  const std::vector<char> reach = reachableBlocks(n, cfgSuccs(cfg));

  {
    const std::string label =
        cfg.FuncDecl.empty() ? "(no function)" : cfg.FuncDecl;
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
    const BB &bb = cfg.Blocks[i];
    const CfgColors col = cfgBlockColors(cfg, i);

    std::string label = "BB" + std::to_string(i);
    for (const std::string &st : bb.Statements)
      label += "\\l" + dotEscape(st);
    if (bb.Term.Kind != TermKind::Fallthrough)
      label += "\\l" + dotEscape(terminatorText(bb.Term));
    label += "\\l";

    s << "  BB" << i << " [label=\"" << label << "\""
      << ", fillcolor=\"" << col.fill << "\""
      << ", color=\"" << col.stroke << "\""
      << ", fontcolor=\"" << col.text << "\"];\n";
  }
  s << "\n";

  std::vector<DotEdge> edges;
  for (int u = 0; u < n; ++u) {
    for (DotEdge e : terminatorEdges(u, cfg.Blocks[u].Term)) {
      if (e.to < 0 || e.to >= n)
        continue;
      edges.push_back(std::move(e));
    }
  }

  const std::vector<EdgeKind> kinds = classifyEdges(n, edges);

  for (size_t ei = 0; ei < edges.size(); ++ei) {
    const DotEdge &e = edges[ei];
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