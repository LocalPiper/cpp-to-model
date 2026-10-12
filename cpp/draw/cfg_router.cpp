#include "cfg_router.h"
#include <algorithm>
#include <cmath>

namespace cpp2model {
namespace {

constexpr double kMargin    = 30.0;
constexpr double kTrackGap  = 14.0;
constexpr double kEndGap    = 12.0;
constexpr double kStubGap   = 10.0;

struct Point { double x, y; };

Point exit_point(const CfgNodeBox& b, int nth_out, int total_out) {
    const double span = b.w * 0.6;
    const double step = (total_out > 1) ? span / (total_out - 1) : 0.0;
    const double offset = (total_out > 1)
                              ? -span / 2.0 + nth_out * step
                              : 0.0;
    return { b.cx() + offset, b.bottom() };
}

Point entry_point(const CfgNodeBox& b, int nth_in, int total_in) {
    const double span = b.w * 0.6;
    const double step = (total_in > 1) ? span / (total_in - 1) : 0.0;
    const double offset = (total_in > 1)
                              ? -span / 2.0 + nth_in * step
                              : 0.0;
    return { b.cx() + offset, b.top() };
}

}  // namespace

void route_edges(CfgLayout& L) {
    const int n = static_cast<int>(L.boxes.size());

    std::vector<int> out_count(n, 0), in_count(n, 0);
    for (const CfgEdge& e : L.edges) {
        if (e.src >= 0 && e.src < n) out_count[e.src]++;
        if (e.dst >= 0 && e.dst < n) in_count[e.dst]++;
    }
    std::vector<int> out_idx(n, 0), in_idx(n, 0);

    int next_left_track  = 0;
    int next_right_track = 0;
    for (CfgEdge& e : L.edges) {
        if (e.kind != EdgeKind::Back) continue;
        if (e.dst < 0 || e.dst >= n) { e.track = 0; continue; }
        const bool left = L.boxes[e.dst].cx() < L.boxes[e.src].cx();
        e.track = left ? next_left_track++ : next_right_track++;
    }

    for (CfgEdge& e : L.edges) {
        if (e.src < 0 || e.src >= n || e.dst < 0 || e.dst >= n) continue;
        const CfgNodeBox& src = L.boxes[e.src];
        const CfgNodeBox& dst = L.boxes[e.dst];

        const Point p0 = exit_point(src, out_idx[e.src]++, out_count[e.src]);
        const Point p1 = entry_point(dst, in_idx[e.dst]++, in_count[e.dst]);

        e.points.clear();
        e.points.emplace_back(p0.x, p0.y);

        switch (e.kind) {
            case EdgeKind::Forward: {
                e.points.emplace_back(p1.x, p1.y);
                break;
            }
            case EdgeKind::Cross: {
                const double corridor = std::max(src.bottom(), dst.bottom()) + kTrackGap;
                e.points.emplace_back(p0.x, corridor);
                e.points.emplace_back(p1.x, corridor);
                e.points.emplace_back(p1.x, p1.y);
                break;
            }
            case EdgeKind::Back: {
                const bool left = dst.cx() < src.cx();
                const double side_x = left
                    ? kMargin + e.track * kTrackGap
                    : L.width - kMargin - e.track * kTrackGap;
                const double stub_y = src.bottom() + kStubGap;
                e.points.emplace_back(p0.x, stub_y);
                e.points.emplace_back(side_x, stub_y);
                e.points.emplace_back(side_x, p1.y - kEndGap);
                e.points.emplace_back(p1.x, p1.y - kEndGap);
                e.points.emplace_back(p1.x, p1.y);
                break;
            }
        }
    }
}

}  // namespace cpp2model