#include "cfg_drawer.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

#include "../text_utils.h"

namespace cpp2model {
namespace {

constexpr double kMargin   = 30.0;
constexpr double kFuncH    = 46.0;
constexpr double kPadX     = 18.0;
constexpr double kPadY     = 12.0;
constexpr double kLineH    = 18.0;
constexpr double kGapX     = 40.0;
constexpr double kGapY     = 70.0;
constexpr double kMinW     = 150.0;
constexpr double kLayerGap = 40.0;
constexpr double kHeaderH  = 22.0;

double text_w(const std::string& s) {
    return static_cast<double>(s.size()) * 6.2;
}

long long rnd(double v) {
    return static_cast<long long>(std::llround(v));
}

struct BlockBox {
    double w = 0.0;
    double h = 0.0;
    double cx = 0.0;
    double cy = 0.0;
};

std::string terminator_text(const Terminator& t) {
    switch (t.kind) {
        case TermKind::Fallthrough: return "";
        case TermKind::Jump:
            return "jump BB" + std::to_string(t.target);
        case TermKind::Branch:
            return "if " + t.cond + " ? BB" + std::to_string(t.on_true) +
                   " : BB" + std::to_string(t.on_false);
        case TermKind::Switch:
            return "switch " + t.switch_val;
        case TermKind::Return:
            return t.ret_val.empty() ? "return" : "return " + t.ret_val;
    }
    return "";
}

std::vector<std::pair<int, std::string>> out_edges(const BB& bb) {
    std::vector<std::pair<int, std::string>> edges;
    switch (bb.term.kind) {
        case TermKind::Fallthrough:
            break;
        case TermKind::Jump:
            if (bb.term.target >= 0) edges.emplace_back(bb.term.target, "");
            break;
        case TermKind::Branch:
            if (bb.term.on_true  >= 0) edges.emplace_back(bb.term.on_true,  "true");
            if (bb.term.on_false >= 0) edges.emplace_back(bb.term.on_false, "false");
            break;
        case TermKind::Switch:
            for (auto& [val, tgt] : bb.term.cases)
                if (tgt >= 0) edges.emplace_back(tgt, std::to_string(val));
            if (bb.term.default_target >= 0)
                edges.emplace_back(bb.term.default_target, "default");
            break;
        case TermKind::Return:
            break;
    }
    return edges;
}

}  // namespace

// evil renderer of doom and despair
std::string CfgDrawer::render(const Cfg& cfg) const {
    const std::string func_label =
        cfg.func_decl.empty() ? "(no function)" : cfg.func_decl;
    const double func_w = std::max(kMinW, text_w(func_label) + 2.0 * kPadX);

    const int n = static_cast<int>(cfg.blocks.size());

    // computing block sizes
    std::vector<BlockBox> box(n);
    for (int i = 0; i < n; ++i) {
        const BB& bb = cfg.blocks[i];
        const bool has_term = (bb.term.kind != TermKind::Fallthrough);
        const std::string ttext = terminator_text(bb.term);

        double w = kMinW;
        for (const std::string& st : bb.statements)
            w = std::max(w, text_w(st) + 2.0 * kPadX);
        if (has_term)
            w = std::max(w, text_w(ttext) + 2.0 * kPadX);

        const size_t stmt_lines = bb.statements.size();
        const size_t term_lines = has_term ? 1 : 0;
        const size_t lines = std::max<size_t>(1, stmt_lines + term_lines);

        box[i].w = w;
        box[i].h = kHeaderH + static_cast<double>(lines) * kLineH + kPadY;
    }

    // bfs depth from entry
    std::vector<int> depth(n, -1);
    std::queue<int> q;
    if (n > 0) { depth[0] = 0; q.push(0); }
    while (!q.empty()) {
        int u = q.front(); q.pop();
        for (auto& [v, _] : out_edges(cfg.blocks[u])) {
            if (v < 0 || v >= n) continue;
            if (depth[v] == -1) {
                depth[v] = depth[u] + 1;
                q.push(v);
            }
        }
    }
    int max_depth = 0;
    for (int i = 0; i < n; ++i)
        if (depth[i] > max_depth) max_depth = depth[i];
    for (int i = 0; i < n; ++i)
        if (depth[i] < 0) depth[i] = max_depth + 1;

    // group by layer
    std::vector<std::vector<int>> layers(max_depth + 2);
    for (int i = 0; i < n; ++i) layers[depth[i]].push_back(i);

    // layer sizes and positions
    std::vector<double> layer_w(layers.size(), 0.0);
    std::vector<double> layer_h(layers.size(), 0.0);
    for (size_t L = 0; L < layers.size(); ++L) {
        double w = 0.0;
        double h = 0.0;
        for (size_t k = 0; k < layers[L].size(); ++k) {
            if (k) w += kGapX;
            int i = layers[L][k];
            w += box[i].w;
            h = std::max(h, box[i].h);
        }
        layer_w[L] = w;
        layer_h[L] = h;
    }

    double canvas_w = func_w;
    for (double w : layer_w) canvas_w = std::max(canvas_w, w);
    canvas_w += 2.0 * kMargin;

    const double func_cx = canvas_w / 2.0;
    const double func_y  = kMargin;
    const double layers_top = func_y + kFuncH + kGapY;

    std::vector<double> layer_y(layers.size(), 0.0);
    double y_cursor = layers_top;
    for (size_t L = 0; L < layers.size(); ++L) {
        layer_y[L] = y_cursor;
        y_cursor += layer_h[L];
        if (L + 1 < layers.size()) y_cursor += kLayerGap;
    }
    const double canvas_h = y_cursor + kMargin;

    for (size_t L = 0; L < layers.size(); ++L) {
        double x = (canvas_w - layer_w[L]) / 2.0;
        for (int i : layers[L]) {
            box[i].cx = x + box[i].w / 2.0;
            box[i].cy = layer_y[L] + layer_h[L] / 2.0;
            x += box[i].w + kGapX;
        }
    }

    // emit svg
    std::ostringstream s;
    s << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << rnd(canvas_w)
      << "\" height=\"" << rnd(canvas_h) << "\" viewBox=\"0 0 " << rnd(canvas_w)
      << " " << rnd(canvas_h)
      << "\" font-family=\"ui-monospace,Menlo,Consolas,monospace\" font-size=\"11\">\n";
    s << "<rect x=\"0\" y=\"0\" width=\"" << rnd(canvas_w)
      << "\" height=\"" << rnd(canvas_h) << "\" fill=\"#ffffff\"/>\n";

    auto arrow = [&](double x1, double y1, double x2, double y2,
                     const std::string& label, bool back_edge) {
        const double dx = x2 - x1;
        const double dy = y2 - y1;
        const double len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-6) return;
        const double ux = dx / len, uy = dy / len;
        const double px = -uy, py = ux;
        constexpr double ah = 9.0;
        constexpr double aw = 4.5;
        const double base_x = x2 - ux * ah;
        const double base_y = y2 - uy * ah;

        s << "<line x1=\"" << rnd(x1) << "\" y1=\"" << rnd(y1)
          << "\" x2=\"" << rnd(base_x) << "\" y2=\"" << rnd(base_y)
          << "\" stroke=\"" << (back_edge ? "#b71c1c" : "#546e7a")
          << "\" stroke-width=\"1.5\""
          << (back_edge ? " stroke-dasharray=\"4 3\"" : "")
          << "/>\n";
        s << "<polygon points=\"" << rnd(x2) << "," << rnd(y2) << " "
          << rnd(base_x + px * aw) << "," << rnd(base_y + py * aw) << " "
          << rnd(base_x - px * aw) << "," << rnd(base_y - py * aw)
          << "\" fill=\"" << (back_edge ? "#b71c1c" : "#546e7a") << "\"/>\n";
        if (!label.empty()) {
            const double mx = (x1 + x2) / 2.0;
            const double my = (y1 + y2) / 2.0;
            s << "<text x=\"" << rnd(mx) << "\" y=\"" << rnd(my - 6)
              << "\" text-anchor=\"middle\" fill=\""
              << (back_edge ? "#b71c1c" : "#37474f")
              << "\" font-size=\"10\">" << xml_escape(label) << "</text>\n";
        }
    };

    auto edge_point = [&](const BlockBox& b, double tx, double ty) {
        const double hw = b.w / 2.0, hh = b.h / 2.0;
        double dx = tx - b.cx, dy = ty - b.cy;
        if (std::abs(dx) < 1e-9 && std::abs(dy) < 1e-9)
            return std::pair{b.cx, b.cy};
        const double sx = (std::abs(dx) > 1e-9) ? hw / std::abs(dx) : 1e9;
        const double sy = (std::abs(dy) > 1e-9) ? hh / std::abs(dy) : 1e9;
        const double t = std::min(sx, sy);
        return std::pair{b.cx + dx * t, b.cy + dy * t};
    };

    // function header
    s << "<g title=\"" << xml_escape("function " + func_label) << "\">\n";
    s << "<rect x=\"" << rnd(func_cx - func_w / 2.0) << "\" y=\"" << rnd(func_y)
      << "\" width=\"" << rnd(func_w) << "\" height=\"" << rnd(kFuncH)
      << "\" rx=\"8\" fill=\"#e3f2fd\" stroke=\"#1e88e5\" stroke-width=\"1.4\"/>\n";
    s << "<text x=\"" << rnd(func_cx) << "\" y=\"" << rnd(func_y + 18)
      << "\" text-anchor=\"middle\" fill=\"#0d47a1\" font-weight=\"bold\">function</text>\n";
    s << "<text x=\"" << rnd(func_cx) << "\" y=\"" << rnd(func_y + kFuncH - 12)
      << "\" text-anchor=\"middle\" fill=\"#0d47a1\" font-size=\"12\">"
      << xml_escape(func_label) << "</text>\n";
    s << "</g>\n";

    if (n > 0) {
        auto [ex, ey] = edge_point(box[0], func_cx, func_y + kFuncH);
        arrow(func_cx, func_y + kFuncH, ex, ey, "", false);
    }

    // edges
    for (int i = 0; i < n; ++i) {
        const BB& bb = cfg.blocks[i];
        std::vector<std::pair<int, std::string>> edges = out_edges(bb);
        if (bb.term.kind == TermKind::Fallthrough && i + 1 < n)
            edges.emplace_back(i + 1, "");

        for (auto& [tgt, label] : edges) {
            if (tgt < 0 || tgt >= n) continue;
            const bool back = depth[tgt] <= depth[i];
            auto [sx, sy] = edge_point(box[i], box[tgt].cx, box[tgt].cy);
            auto [tx, ty] = edge_point(box[tgt], box[i].cx, box[i].cy);
            if (back) {
                const double side_x = (box[tgt].cx < box[i].cx)
                                          ? kMargin / 2.0
                                          : canvas_w - kMargin / 2.0;
                s << "<polyline points=\""
                  << rnd(sx) << "," << rnd(sy) << " "
                  << rnd(side_x) << "," << rnd(sy) << " "
                  << rnd(side_x) << "," << rnd(ty) << " "
                  << rnd(tx)  << "," << rnd(ty)
                  << "\" fill=\"none\" stroke=\"#b71c1c\" stroke-width=\"1.5\""
                  << " stroke-dasharray=\"4 3\"/>\n";
                arrow(side_x, ty, tx, ty, label, true);
            } else {
                arrow(sx, sy, tx, ty, label, false);
            }
        }
    }

    // blocks
    for (int i = 0; i < n; ++i) {
        const BB& bb = cfg.blocks[i];
        const BlockBox& b = box[i];
        const double bx = b.cx - b.w / 2.0;
        const double by = b.cy - b.h / 2.0;

        const char* fill;
        const char* stroke;
        const char* text;
        if (i == 0) {
            fill = "#e3f2fd"; stroke = "#1e88e5"; text = "#0d47a1";
        } else if (bb.term.kind == TermKind::Return) {
            fill = "#fce4ec"; stroke = "#d81b60"; text = "#880e4f";
        } else {
            fill = "#fafafa"; stroke = "#9e9e9e"; text = "#212121";
        }

        s << "<g title=\"" << xml_escape("BB" + std::to_string(i)) << "\">\n";
        s << "<rect x=\"" << rnd(bx) << "\" y=\"" << rnd(by)
          << "\" width=\"" << rnd(b.w) << "\" height=\"" << rnd(b.h)
          << "\" rx=\"8\" fill=\"" << fill << "\" stroke=\"" << stroke
          << "\" stroke-width=\"1.2\"/>\n";
        s << "<text x=\"" << rnd(b.cx) << "\" y=\"" << rnd(by + 15)
          << "\" text-anchor=\"middle\" fill=\"" << text
          << "\" font-weight=\"bold\" font-size=\"10\">BB" << i << "</text>\n";

        const bool has_term = (bb.term.kind != TermKind::Fallthrough);
        double ty = by + kHeaderH + 14.0;

        if (bb.statements.empty() && !has_term) {
            s << "<text x=\"" << rnd(b.cx) << "\" y=\"" << rnd(ty)
              << "\" text-anchor=\"middle\" fill=\"#bdbdbd\" font-style=\"italic\">(empty)</text>\n";
            ty += kLineH;
        } else {
            for (const std::string& st : bb.statements) {
                s << "<text x=\"" << rnd(b.cx) << "\" y=\"" << rnd(ty)
                  << "\" text-anchor=\"middle\" fill=\"" << text << "\">"
                  << xml_escape(st) << "</text>\n";
                ty += kLineH;
            }
            if (has_term) {
                s << "<text x=\"" << rnd(b.cx) << "\" y=\"" << rnd(ty)
                  << "\" text-anchor=\"middle\" fill=\"#5d4037\" font-size=\"10\""
                  << " font-style=\"italic\">"
                  << xml_escape(terminator_text(bb.term)) << "</text>\n";
                ty += kLineH;
            }
        }
        s << "</g>\n";
    }

    if (n == 0) {
        s << "<text x=\"" << rnd(kMargin) << "\" y=\"" << rnd(canvas_h - 12)
          << "\" fill=\"#b71c1c\" font-size=\"11\">no basic blocks produced</text>\n";
    }

    s << "</svg>";
    return s.str();
}

}  // namespace cpp2model