#include "cfg_drawer.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "../text_utils.h"

namespace cpp2model {
namespace {

constexpr double kMargin = 30.0;
constexpr double kFuncH = 46.0;
constexpr double kCondH = 60.0;
constexpr double kPadX = 18.0;
constexpr double kPadY = 12.0;
constexpr double kLineH = 18.0;
constexpr double kGapX = 48.0;
constexpr double kGapY = 64.0;
constexpr double kMinW = 150.0;

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
    bool is_bb = true;
};

}  // namespace

std::string CfgDrawer::render(const Cfg& cfg) const {
    const std::string func_label = cfg.func_decl.empty() ? "(no function)" : cfg.func_decl;
    const double func_w = std::max(kMinW, text_w(func_label) + 2.0 * kPadX);

    std::vector<BlockBox> boxes(cfg.blocks.size());
    double max_blk_h = 0.0;
    double blocks_row_w = 0.0;
    for (size_t i = 0; i < cfg.blocks.size(); ++i) {
        BlockBox& b = boxes[i];
        if (std::holds_alternative<BB>(cfg.blocks[i])) {
            const BB& bb = std::get<BB>(cfg.blocks[i]);
            b.is_bb = true;
            double w = kMinW;
            for (const std::string& st : bb.statements) {
                w = std::max(w, text_w(st) + 2.0 * kPadX);
            }
            b.w = w;
            b.h = std::max(1.0, static_cast<double>(bb.statements.size())) * kLineH
                  + 2.0 * kPadY;
        } else {
            const Condition& c = std::get<Condition>(cfg.blocks[i]);
            b.is_bb = false;
            b.w = std::max(kMinW, text_w(c.cond) + 2.0 * kPadX + 24.0);
            b.h = kCondH;
        }
        max_blk_h = std::max(max_blk_h, b.h);
        if (i) blocks_row_w += kGapX;
        blocks_row_w += b.w;
    }

    const double canvas_w = std::max(func_w, blocks_row_w) + 2.0 * kMargin;
    const double blocks_y = kMargin + kFuncH + kGapY;
    const double canvas_h = blocks_y + max_blk_h + kMargin;

    const double func_cx = canvas_w / 2.0;
    const double func_y = kMargin;

    double x = (canvas_w - blocks_row_w) / 2.0;
    for (size_t i = 0; i < boxes.size(); ++i) {
        boxes[i].cx = x + boxes[i].w / 2.0;
        boxes[i].cy = blocks_y + boxes[i].h / 2.0;
        x += boxes[i].w + kGapX;
    }

    std::ostringstream s;
    s << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << rnd(canvas_w)
      << "\" height=\"" << rnd(canvas_h) << "\" viewBox=\"0 0 " << rnd(canvas_w) << " "
      << rnd(canvas_h)
      << "\" font-family=\"ui-monospace,Menlo,Consolas,monospace\" font-size=\"11\">\n";
    s << "<rect x=\"0\" y=\"0\" width=\"" << rnd(canvas_w) << "\" height=\"" << rnd(canvas_h)
      << "\" fill=\"#ffffff\"/>\n";

    auto arrow = [&](double x1, double y1, double x2, double y2, const std::string& label) {
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
        s << "<line x1=\"" << rnd(x1) << "\" y1=\"" << rnd(y1) << "\" x2=\"" << rnd(base_x)
          << "\" y2=\"" << rnd(base_y) << "\" stroke=\"#546e7a\" stroke-width=\"1.5\"/>\n";
        s << "<polygon points=\"" << rnd(x2) << "," << rnd(y2) << " "
          << rnd(base_x + px * aw) << "," << rnd(base_y + py * aw) << " "
          << rnd(base_x - px * aw) << "," << rnd(base_y - py * aw)
          << "\" fill=\"#546e7a\"/>\n";
        if (!label.empty()) {
            const double mx = (x1 + x2) / 2.0;
            const double my = (y1 + y2) / 2.0;
            s << "<text x=\"" << rnd(mx) << "\" y=\"" << rnd(my - 6)
              << "\" text-anchor=\"middle\" fill=\"#37474f\" font-size=\"10\">"
              << xml_escape(label) << "</text>\n";
        }
    };

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

    if (!boxes.empty()) {
        arrow(func_cx, func_y + kFuncH, boxes[0].cx, boxes[0].cy - boxes[0].h / 2.0, "");
        for (size_t i = 0; i < cfg.blocks.size(); ++i) {
            if (!std::holds_alternative<Condition>(cfg.blocks[i])) continue;
            const Condition& c = std::get<Condition>(cfg.blocks[i]);
            const double sx = boxes[i].cx;
            const double sy = boxes[i].cy + boxes[i].h / 2.0;
            if (c.on_true >= 0 && c.on_true < static_cast<int>(boxes.size())) {
                arrow(sx, sy, boxes[c.on_true].cx,
                      boxes[c.on_true].cy - boxes[c.on_true].h / 2.0, "true");
            }
            if (c.on_false >= 0 && c.on_false < static_cast<int>(boxes.size())) {
                arrow(sx, sy, boxes[c.on_false].cx,
                      boxes[c.on_false].cy - boxes[c.on_false].h / 2.0, "false");
            }
        }
    }

    for (size_t i = 0; i < boxes.size(); ++i) {
        const BlockBox& b = boxes[i];
        const double bx = b.cx - b.w / 2.0;
        const double by = b.cy - b.h / 2.0;
        if (b.is_bb) {
            const BB& bb = std::get<BB>(cfg.blocks[i]);
            s << "<g title=\"" << xml_escape("BB" + std::to_string(i)) << "\">\n";
            s << "<rect x=\"" << rnd(bx) << "\" y=\"" << rnd(by) << "\" width=\"" << rnd(b.w)
              << "\" height=\"" << rnd(b.h)
              << "\" rx=\"8\" fill=\"#e8f5e9\" stroke=\"#43a047\" stroke-width=\"1.2\"/>\n";
            s << "<text x=\"" << rnd(b.cx) << "\" y=\"" << rnd(by + 15)
              << "\" text-anchor=\"middle\" fill=\"#1b5e20\" font-weight=\"bold\" font-size=\"10\">BB"
              << i << "</text>\n";
            if (bb.statements.empty()) {
                s << "<text x=\"" << rnd(b.cx) << "\" y=\"" << rnd(by + kPadY + 16.0)
                  << "\" text-anchor=\"middle\" fill=\"#90a4ae\" font-style=\"italic\">(empty)</text>\n";
            } else {
                double ty = by + kPadY + 16.0;
                for (const std::string& st : bb.statements) {
                    s << "<text x=\"" << rnd(b.cx) << "\" y=\"" << rnd(ty)
                      << "\" text-anchor=\"middle\" fill=\"#1b5e20\">"
                      << xml_escape(st) << "</text>\n";
                    ty += kLineH;
                }
            }
            s << "</g>\n";
        } else {
            const Condition& c = std::get<Condition>(cfg.blocks[i]);
            s << "<g title=\"" << xml_escape("condition " + c.cond) << "\">\n";
            s << "<polygon points=\"" << rnd(b.cx) << "," << rnd(by) << " "
              << rnd(b.cx + b.w / 2.0) << "," << rnd(b.cy) << " " << rnd(b.cx) << ","
              << rnd(by + b.h) << " " << rnd(b.cx - b.w / 2.0) << "," << rnd(b.cy)
              << "\" fill=\"#fff8e1\" stroke=\"#f9a825\" stroke-width=\"1.2\"/>\n";
            s << "<text x=\"" << rnd(b.cx) << "\" y=\"" << rnd(b.cy + 4)
              << "\" text-anchor=\"middle\" fill=\"#5d4037\">" << xml_escape(c.cond)
              << "</text>\n";
            s << "</g>\n";
        }
    }

    if (cfg.blocks.empty()) {
        s << "<text x=\"" << rnd(kMargin) << "\" y=\"" << rnd(canvas_h - 12)
          << "\" fill=\"#b71c1c\" font-size=\"11\">no basic blocks produced</text>\n";
    }

    s << "</svg>";
    return s.str();
}

}  // namespace cpp2model
