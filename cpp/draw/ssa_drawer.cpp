#include "ssa_drawer.h"
#include "ssa_dot.h"
#include "dot_runner.h"

namespace cpp2model {

std::string SsaDrawer::render(const SsaCfg &ssa) const {
  if (ssa.blocks.empty())
    return "";
  std::string err;
  std::string svg = dotToSvg(ssaToDot(ssa), err);
  return svg.empty() ? ("<!-- dot error: " + err + " -->") : svg;
}

} // namespace cpp2model