#pragma once
#include <string>
#include <vector>
#include "../model.h"

namespace cpp2model {

struct AstNodeBox {
    int    node        = -1;
    int    parent      = -1;
    int    depth       = 0;
    double span        = 1.0;
    double cx          = 0.0;
    double w           = 70.0;
    int    y           = 0;
};

struct AstLayout {
    std::vector<AstNodeBox> boxes;
    double width  = 0.0;
    double height = 0.0;
};

AstLayout layout_ast(const TreeModel& tree);

}  // namespace cpp2model