#include "dot_common.h"

#include <functional>

namespace cpp2model {

std::string terminatorText(const Terminator &t) {
  switch (t.Kind) {
  case TermKind::Fallthrough:
    return "";
  case TermKind::Jump:
    return "jump BB" + std::to_string(t.Target);
  case TermKind::Branch:
    return "if " + t.Cond + " ? BB" + std::to_string(t.OnTrue) + " : BB" +
           std::to_string(t.OnFalse);
  case TermKind::Switch:
    return "switch " + t.SwitchVal;
  case TermKind::Return:
    return t.RetVal.empty() ? "return" : "return " + t.RetVal;
  }
  return "";
}

std::vector<DotEdge> terminatorEdges(int block, const Terminator &t) {
  std::vector<DotEdge> out;
  switch (t.Kind) {
  case TermKind::Fallthrough:
    break;
  case TermKind::Jump:
    if (t.Target >= 0)
      out.push_back({block, t.Target, "", false});
    break;
  case TermKind::Branch:
    if (t.OnTrue >= 0)
      out.push_back({block, t.OnTrue, "true", true});
    if (t.OnFalse >= 0)
      out.push_back({block, t.OnFalse, "false", true});
    break;
  case TermKind::Switch:
    for (auto &[v, tgt] : t.Cases)
      if (tgt >= 0)
        out.push_back({block, tgt, std::to_string(v), true});
    if (t.DefaultTarget >= 0)
      out.push_back({block, t.DefaultTarget, "default", true});
    break;
  case TermKind::Return:
    break;
  }
  return out;
}

std::vector<char> reachableBlocks(int numBlocks,
                                  const std::vector<std::vector<int>> &succs) {
  std::vector<char> seen(numBlocks, 0);
  if (numBlocks == 0)
    return seen;
  std::vector<int> stack = {0};
  seen[0] = 1;
  while (!stack.empty()) {
    int u = stack.back();
    stack.pop_back();
    for (int v : succs[u]) {
      if (v < 0 || v >= numBlocks)
        continue;
      if (!seen[v]) {
        seen[v] = 1;
        stack.push_back(v);
      }
    }
  }
  return seen;
}

std::vector<EdgeKind> classifyEdges(int numBlocks,
                                    const std::vector<DotEdge> &edges) {
  std::vector<std::vector<int>> outEdges(numBlocks);
  for (size_t ei = 0; ei < edges.size(); ++ei) {
    int u = edges[ei].from;
    if (u >= 0 && u < numBlocks)
      outEdges[u].push_back(static_cast<int>(ei));
  }

  enum { White, Gray, Black };
  std::vector<char> color(numBlocks, White);
  std::vector<char> isBack(edges.size(), 0);

  std::function<void(int)> dfs = [&](int u) {
    color[u] = Gray;
    for (int ei : outEdges[u]) {
      int v = edges[ei].to;
      if (v < 0 || v >= numBlocks)
        continue;
      if (color[v] == Gray)
        isBack[ei] = 1;
      else if (color[v] == White)
        dfs(v);
    }
    color[u] = Black;
  };
  for (int i = 0; i < numBlocks; ++i)
    if (color[i] == White)
      dfs(i);

  std::vector<int> depth(numBlocks, 0);
  for (int iter = 0; iter < numBlocks; ++iter) {
    bool changed = false;
    for (size_t ei = 0; ei < edges.size(); ++ei) {
      if (isBack[ei])
        continue;
      int u = edges[ei].from, v = edges[ei].to;
      if (u < 0 || v < 0 || u >= numBlocks || v >= numBlocks)
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
    if (isBack[ei]) {
      kinds[ei] = EdgeKind::Back;
    } else {
      int u = edges[ei].from, v = edges[ei].to;
      kinds[ei] = (v >= 0 && v < numBlocks && depth[v] > depth[u])
                      ? EdgeKind::Forward
                      : EdgeKind::Cross;
    }
  }
  return kinds;
}

CfgEdgeColors cfgEdgeColors(EdgeKind kind, bool isConditional) {
  CfgEdgeColors c;
  switch (kind) {
  case EdgeKind::Forward:
    c.color = isConditional ? "#546e7a" : "#78909c";
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

} // namespace cpp2model