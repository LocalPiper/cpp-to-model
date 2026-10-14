#include "cfg_drawer.h"
#include "dot_runner.h"
#include "dotgen.h"

namespace cpp2model {

std::string CfgDrawer::render(const Cfg &cfg) const {
  if (cfg.blocks.empty())
    return "";
  std::string err;
  std::string svg = dot_to_svg(cfg_to_dot(cfg), err);
  return svg.empty() ? ("<!-- dot error: " + err + " -->") : svg;
}

} // namespace cpp2model