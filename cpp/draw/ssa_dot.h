#pragma once

#include <string>

#include "../ssa/ssa_builder.h"
#include "dot_common.h"

namespace cpp2model {

CfgColors ssaBlockColors(const SsaCfg &ssa, int bbIndex);

std::string ssaToDot(const SsaCfg &ssa);

} // namespace cpp2model