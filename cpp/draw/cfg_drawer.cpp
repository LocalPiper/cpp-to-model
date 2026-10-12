#include "cfg_drawer.h"
#include "cfg_layout.h"
#include "cfg_router.h"
#include "cfg_painter.h"

namespace cpp2model {

std::string CfgDrawer::render(const Cfg& cfg) const {
    if (cfg.blocks.empty()) return "";
    CfgLayout layout = layout_cfg(cfg);
    route_edges(layout);
    return paint_cfg_svg(cfg, layout);
}

}  // namespace cpp2model