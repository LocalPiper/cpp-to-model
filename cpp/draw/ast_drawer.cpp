#include "ast_drawer.h"
#include "ast_dot.h"
#include "dot_runner.h"

namespace cpp2model {

std::string AstDrawer::render(const TreeModel &tree) const {
  if (tree.nodes.empty())
    return "";
  std::string err;
  std::string svg = dotToSvg(astToDot(tree), err);
  return svg.empty() ? ("<!-- dot error: " + err + " -->") : svg;
}

} // namespace cpp2model