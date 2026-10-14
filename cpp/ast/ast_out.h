#pragma once

#include <iosfwd>

#include "../model.h"

namespace cpp2model {

void emitTreeJson(std::ostream &os, const TreeModel &tree);

} // namespace cpp2model