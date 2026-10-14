#pragma once

#include <string>
#include <vector>

#include "../cfg/cfg_builder.h"

namespace cpp2model {

enum class EdgeKind { Forward, Back, Cross };

struct DotEdge {
  int from = -1;
  int to = -1;
  std::string label;
  bool conditional = false;
};

struct CfgColors {
  std::string fill;
  std::string stroke;
  std::string text;
};

struct CfgEdgeColors {
  std::string color;
  std::string fontcolor;
};

std::string terminatorText(const Terminator &t);

std::vector<DotEdge> terminatorEdges(int block, const Terminator &t);

std::vector<char> reachableBlocks(int numBlocks,
                                  const std::vector<std::vector<int>> &succs);

std::vector<EdgeKind> classifyEdges(int numBlocks,
                                    const std::vector<DotEdge> &edges);

CfgEdgeColors cfgEdgeColors(EdgeKind kind, bool isConditional);

} // namespace cpp2model