#pragma once
#include <string>
#include "../cfg/cfg_builder.h"
#include "cfg_layout.h"

namespace cpp2model {

std::string paint_cfg_svg(const Cfg& cfg, const CfgLayout& layout);

}  // namespace cpp2model