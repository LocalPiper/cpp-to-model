#include "cfg_drawer.h"
#include "cfg_dot.h"
#include "dot_runner.h"

namespace cpp2model {

std::string CfgDrawer::render(const Cfg &cfg) const {
  if (cfg.Blocks.empty())
    return "";
  std::string err;
  std::string svg = dotToSvg(cfgToDot(cfg), err);
  return svg.empty() ? ("<!-- dot error: " + err + " -->") : svg;
}

} // namespace cpp2model