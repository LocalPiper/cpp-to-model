#pragma once

#include <iosfwd>

#include "cfg_builder.h"

namespace cpp2model {

void emitCfgJson(std::ostream &os, const Cfg &cfg);

} // namespace cpp2model
