#include "ast_layout.h"
#include <algorithm>
#include <functional>

namespace cpp2model {
namespace {

constexpr int    kNodeH   = 34;
constexpr int    kPitch   = 64;
constexpr double kMinUnit = 64.0;
constexpr double kMinGap  = 24.0;
constexpr double kMargin  = 30.0;

double node_width(const Node& n) {
    double wk = static_cast<double>(n.kind.size())   * 6.2 + 14.0;
    double wd = n.detail.empty()
                    ? 0.0
                    : static_cast<double>(n.detail.size()) * 6.2 + 14.0;
    return std::max(70.0, std::max(wk, wd));
}

}  // namespace

AstLayout layout_ast(const TreeModel& tree) {
    AstLayout L;
    const std::vector<Node>& nodes = tree.nodes;
    if (nodes.empty()) return L;

    L.boxes.resize(nodes.size());

    std::function<void(int, int)> compute = [&](int i, int depth) {
        AstNodeBox& b = L.boxes[i];
        b.node = i;
        b.depth = depth;
        b.w = node_width(nodes[i]);
        double s = 0.0;
        for (int c : nodes[i].children) {
            L.boxes[c].parent = i;
            compute(c, depth + 1);
            s += L.boxes[c].span;
        }
        b.span = std::max(1.0, s);
    };
    compute(0, 0);

    double max_w = 0.0;
    for (const AstNodeBox& b : L.boxes) max_w = std::max(max_w, b.w);
    double unit = std::max(kMinUnit, max_w + 30.0);

    std::function<void(int, double)> assign = [&](int i, double left_unit) {
        AstNodeBox& b = L.boxes[i];
        const Node& n = nodes[i];
        if (n.children.empty()) {
            b.cx = (left_unit + 0.5) * unit;
            return;
        }
        double cur = left_unit;
        for (int c : n.children) {
            assign(c, cur);
            cur += L.boxes[c].span;
        }
        b.cx = (left_unit + cur) / 2.0 * unit;
    };
    assign(0, 0.0);

    int max_depth = 0;
    for (const AstNodeBox& b : L.boxes) max_depth = std::max(max_depth, b.depth);

    std::function<void(int, double)> shift = [&](int i, double dx) {
        L.boxes[i].cx += dx;
        for (int c : nodes[i].children) shift(c, dx);
    };

    for (int iter = 0; iter < 2; ++iter) {
        for (int d = 0; d <= max_depth; ++d) {
            std::vector<int> ids;
            for (size_t i = 0; i < nodes.size(); ++i)
                if (L.boxes[i].depth == d) ids.push_back(static_cast<int>(i));
            std::sort(ids.begin(), ids.end(),
                      [&](int a, int b) { return L.boxes[a].cx < L.boxes[b].cx; });
            double prev_right = -1e18;
            for (int id : ids) {
                double left = L.boxes[id].cx - L.boxes[id].w / 2.0;
                double need = prev_right + kMinGap;
                if (left < need) shift(id, need - left);
                prev_right = L.boxes[id].cx + L.boxes[id].w / 2.0;
            }
        }
    }

    for (AstNodeBox& b : L.boxes)
        b.y = b.depth * kPitch + static_cast<int>(kMargin);

    L.width = kMargin * 2.0;
    for (const AstNodeBox& b : L.boxes)
        L.width = std::max(L.width, b.cx + b.w / 2.0 + kMargin);
    L.height = static_cast<double>(max_depth + 1) * kPitch
               + kNodeH + kMargin * 2.0;
    return L;
}

}  // namespace cpp2model