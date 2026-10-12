#pragma once
#include <string>
#include "../model.h"
#include "ast_layout.h"

namespace cpp2model {

std::string paint_ast_svg(const TreeModel& tree, const AstLayout& layout);

}  // namespace cpp2model