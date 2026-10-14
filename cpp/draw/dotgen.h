#pragma once
#include <string>
#include "../model.h"
#include "../cfg/cfg_builder.h"
#include "../ssa/ssa_builder.h"

namespace cpp2model {

enum class EdgeKind { Forward, Back, Cross };

std::string dot_escape(const std::string& s);

struct CfgColors {
    std::string fill;
    std::string stroke;
    std::string text;
};
CfgColors cfg_block_colors(const Cfg& cfg, int bb_index);

struct CfgEdgeColors {
    std::string color;
    std::string fontcolor;
};
CfgEdgeColors cfg_edge_colors(EdgeKind kind, bool is_conditional);

std::string cfg_to_dot(const Cfg& cfg);
std::string ast_to_dot(const TreeModel& tree);
std::string ssa_to_dot(const SsaCfg& ssa);
}  // namespace cpp2model