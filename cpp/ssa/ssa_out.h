#pragma once

#include <iosfwd>

#include "ssa_builder.h"

namespace cpp2model {

void emitSsaJson(std::ostream &os, const SsaCfg &cfg);

} // namespace cpp2model