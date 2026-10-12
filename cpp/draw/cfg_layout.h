#pragma once
#include <string>
#include <vector>
#include "../cfg/cfg_builder.h"

namespace cpp2model {

enum class EdgeKind { Forward, Back, Cross };

struct CfgNodeBox {
    int    block = -1;
    int    layer = 0;
    int    pos_in_layer = 0;
    double x = 0, y = 0;
    double w = 0, h = 0;

    double cx() const { return x + w / 2; }
    double cy() const { return y + h / 2; }
    double bottom() const { return y + h; }
    double top()    const { return y; }
    double left()   const { return x; }
    double right()  const { return x + w; }
};

struct CfgEdge {
    int    src   = -1;
    int    dst   = -1;
    std::string label;
    EdgeKind kind = EdgeKind::Forward;
    int    track  = 0;
    std::vector<std::pair<double,double>> points;
};

struct CfgLayout {
    std::string func_decl;
    std::vector<CfgNodeBox> boxes;
    std::vector<CfgEdge>    edges;
    double width  = 0.0;
    double height = 0.0;
    double func_x = 0, func_y = 0, func_w = 0, func_h = 0;
};

CfgLayout layout_cfg(const Cfg& cfg);

}  // namespace cpp2model