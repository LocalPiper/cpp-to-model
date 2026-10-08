#pragma once

#include <iosfwd>

#include "model.h"

namespace cpp2model {

void emit_tree_json(std::ostream& os, const TreeModel& tree);

}  // namespace cpp2model