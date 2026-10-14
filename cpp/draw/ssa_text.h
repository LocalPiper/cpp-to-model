#pragma once

#include <string>

#include "../ssa/ssa_builder.h"

namespace cpp2model {

std::string ssaTacText(const TacOp &op);

std::string ssaPhiText(const PhiNode &phi);

} // namespace cpp2model