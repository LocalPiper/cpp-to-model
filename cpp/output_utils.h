#pragma once

#include <iosfwd>

#include "cfg/cfg_builder.h"

namespace cpp2model {

const char *termKindName(TermKind k);

void emitTerminatorJson(std::ostream &os, const Terminator &t);

} // namespace cpp2model