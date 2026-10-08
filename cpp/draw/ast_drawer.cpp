#include "ast_drawer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "../text_utils.h"

namespace cpp2model {
namespace {

const char* fill_for(NodeCategory cat) {
    switch (cat) {
        case NodeCategory::Root:        return "#263238";
        case NodeCategory::Declaration: return "#e3f2fd";
        case NodeCategory::Statement:   return "#e8f5e9";
        case NodeCategory::Expression:  return "#fff8e1";
    }
    return "#eceff1";
}

const char* stroke_for(NodeCategory cat) {
    switch (cat) {
        case NodeCategory::Root:        return "#37474f";
        case NodeCategory::Declaration: return "#1e88e5";
        case NodeCategory::Statement:   return "#43a047";
        case NodeCategory::Expression:  return "#f9a825";
    }
    return "#78909c";
}

const char* text_for(NodeCategory cat) {
    switch (cat) {
        case NodeCategory::Root:        return "#ffffff";
        case NodeCategory::Declaration: return "#0d47a1";
        case NodeCategory::Statement:   return "#1b5e20";
        case NodeCategory::Expression:  return "#5d4037";
    }
    return "#37474f";
}

struct L {
    int depth = 0;
    int parent = -1;
    double span = 1.0;
    double cx = 0.0;
    double w = 70.0;
};

}  // namespace

std::string AstDrawer::render(const TreeModel& tree) const {
    const std::vector<Node>& nodes = tree.nodes;
    if (nodes.empty()) return "";
    constexpr int kNodeH = 34;
    constexpr int kPitch = 64;
    constexpr double kMinUnit = 64.0;
    constexpr double kMinGap = 24.0;
    constexpr double kMargin = 30.0;

    std::vector<L> lv(nodes.size());

    auto node_width = [&](int i) {
        double wk = static_cast<double>(nodes[i].kind.size()) * 6.2 + 14.0;
        double wd = nodes[i].detail.empty()
                        ? 0.0
                        : static_cast<double>(nodes[i].detail.size()) * 6.2 + 14.0;
        return std::max(70.0, std::max(wk, wd));
    };

    std::function<void(int, int)> compute = [&](int i, int depth) {
        lv[i].depth = depth;
        double s = 0.0;
        for (int c : nodes[i].children) {
            lv[c].parent = i;
            compute(c, depth + 1);
            s += lv[c].span;
        }
        lv[i].span = std::max(1.0, s);
        lv[i].w = node_width(i);
    };
    compute(0, 0);

    double max_w = 0.0;
    for (size_t i = 0; i < nodes.size(); ++i) max_w = std::max(max_w, lv[i].w);
    double unit = std::max(kMinUnit, max_w + 30.0);

    std::function<void(int, double)> assign = [&](int i, double left_unit) {
        if (nodes[i].children.empty()) {
            lv[i].cx = (left_unit + 0.5) * unit;
            return;
        }
        double cur = left_unit;
        for (int c : nodes[i].children) {
            assign(c, cur);
            cur += lv[c].span;
        }
        lv[i].cx = (left_unit + cur) / 2.0 * unit;
    };
    assign(0, 0.0);

    std::function<void(int, double)> shift = [&](int i, double dx) {
        lv[i].cx += dx;
        for (int c : nodes[i].children) shift(c, dx);
    };

    int max_depth = 0;
    for (size_t i = 0; i < nodes.size(); ++i) max_depth = std::max(max_depth, lv[i].depth);


    for (int iter = 0; iter < 2; ++iter) {
        for (int d = 0; d <= max_depth; ++d) {
            std::vector<int> ids;
            for (size_t i = 0; i < nodes.size(); ++i)
                if (lv[i].depth == d) ids.push_back(static_cast<int>(i));
            std::sort(ids.begin(), ids.end(),
                      [&](int a, int b) { return lv[a].cx < lv[b].cx; });
            double prev_right = -1e18;
            for (int id : ids) {
                double left = lv[id].cx - lv[id].w / 2.0;
                double need = prev_right + kMinGap;
                if (left < need) shift(id, need - left);
                prev_right = lv[id].cx + lv[id].w / 2.0;
            }
        }
    }

    double width = kMargin * 2.0;
    for (size_t i = 0; i < nodes.size(); ++i)
        width = std::max(width, lv[i].cx + lv[i].w / 2.0 + kMargin);
    double height = static_cast<double>(max_depth + 1) * kPitch + kNodeH + kMargin * 2.0;

    auto y_of = [&](int i) { return static_cast<double>(lv[i].depth) * kPitch + kMargin; };
    auto rx = [](double v) { return static_cast<long long>(std::llround(v)); };

    std::ostringstream s;
    s << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << rx(width)
      << "\" height=\"" << rx(height) << "\" viewBox=\"0 0 " << rx(width) << " "
      << rx(height) << "\" font-family=\"ui-monospace,Menlo,Consolas,monospace\" font-size=\"11\">\n";
    s << "<rect x=\"0\" y=\"0\" width=\"" << rx(width) << "\" height=\"" << rx(height)
      << "\" fill=\"#ffffff\"/>\n";

    for (size_t i = 1; i < nodes.size(); ++i) {
        int p = lv[i].parent;
        if (p < 0) continue;
        s << "<line x1=\"" << rx(lv[p].cx) << "\" y1=\"" << rx(y_of(p) + kNodeH)
          << "\" x2=\"" << rx(lv[i].cx) << "\" y2=\"" << rx(y_of(static_cast<int>(i)))
          << "\" stroke=\"#90a4ae\" stroke-width=\"1.5\"/>\n";
    }

    for (size_t i = 0; i < nodes.size(); ++i) {
        const Node& n = nodes[i];
        double nx = lv[i].cx - lv[i].w / 2.0;
        double ny = y_of(static_cast<int>(i));

        std::string title = n.kind;
        if (!n.detail.empty()) title += " | " + n.detail;
        if (!n.type.empty()) title += " | " + n.type;
        if (!n.range.empty()) title += " | " + n.range;

        s << "<g title=\"" << xml_escape(title) << "\">\n";
        s << "<rect x=\"" << rx(nx) << "\" y=\"" << rx(ny) << "\" width=\"" << rx(lv[i].w)
          << "\" height=\"" << kNodeH << "\" rx=\"6\" fill=\"" << fill_for(n.cat)
          << "\" stroke=\"" << stroke_for(n.cat) << "\" stroke-width=\"1.2\"/>\n";
        s << "<text x=\"" << rx(lv[i].cx) << "\" y=\"" << rx(ny + 14)
          << "\" text-anchor=\"middle\" fill=\"" << text_for(n.cat)
          << "\" font-weight=\"bold\">" << xml_escape(n.kind) << "</text>\n";
        if (!n.detail.empty()) {
            s << "<text x=\"" << rx(lv[i].cx) << "\" y=\"" << rx(ny + 27)
              << "\" text-anchor=\"middle\" fill=\"" << text_for(n.cat) << "\">"
              << xml_escape(n.detail) << "</text>\n";
        }
        s << "</g>\n";
    }

    if (tree.truncated) {
        s << "<text x=\"" << rx(kMargin) << "\" y=\"" << rx(height - 10)
          << "\" fill=\"#b71c1c\" font-size=\"12\">... AST truncated (node limit reached)</text>\n";
    }
    s << "</svg>";
    return s.str();
}

}  // namespace cpp2model