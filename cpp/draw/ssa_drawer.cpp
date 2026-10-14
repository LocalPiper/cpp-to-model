#include "ssa_drawer.h"
#include "dot_runner.h"
#include "dotgen.h"

namespace cpp2model {

std::string SsaDrawer::render(const SsaCfg &ssa) const {
  if (ssa.blocks.empty())
    return "";
  std::string err;
  std::string svg = dot_to_svg(ssa_to_dot(ssa), err);
  return svg.empty() ? ("<!-- dot error: " + err + " -->") : svg;
}

} // namespace cpp2model