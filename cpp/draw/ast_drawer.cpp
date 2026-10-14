#include "ast_drawer.h"
#include "dot_runner.h"
#include "dotgen.h"

namespace cpp2model {

std::string AstDrawer::render(const TreeModel &tree) const {
  if (tree.nodes.empty())
    return "";
  std::string err;
  std::string svg = dot_to_svg(ast_to_dot(tree), err);
  return svg.empty() ? ("<!-- dot error: " + err + " -->") : svg;
}

} // namespace cpp2model