#include "ast_drawer.h"
#include "ast_layout.h"
#include "ast_painter.h"

namespace cpp2model {

std::string AstDrawer::render(const TreeModel& tree) const {
    if (tree.nodes.empty()) return "";
    const AstLayout layout = layout_ast(tree);
    return paint_ast_svg(tree, layout);
}

}  // namespace cpp2model