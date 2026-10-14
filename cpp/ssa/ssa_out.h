#pragma once

#include "ssa_builder.h"

#include <iosfwd>

namespace cpp2model {

void emit_ssa_json(std::ostream& os, const SsaCfg& cfg);

} // namespace cpp2model