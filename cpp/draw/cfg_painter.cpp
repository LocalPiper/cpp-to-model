#include "cfg_painter.h"
#include <cmath>
#include <sstream>
#include "../text_utils.h"
#include "../cfg/cfg_builder.h"

namespace cpp2model {
namespace {

constexpr double kHeaderH = 22.0;
constexpr double kLineH   = 18.0;

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

const char* edge_color(EdgeKind k) {
    switch (k) {
        case EdgeKind::Forward: return "#546e7a";
        case EdgeKind::Cross:   return "#ef6c00";
        case EdgeKind::Back:    return "#b71c1c";
    }
    return "#546e7a";
}

long long rx(double v) { return static_cast<long long>(std::llround(v)); }

void emit_arrowhead(std::ostringstream& s,
                    double x1, double y1, double x2, double y2,
                    const char* color) {
    const double dx = x2 - x1, dy = y2 - y1;
    const double len = std::sqrt(dx*dx + dy*dy);
    if (len < 1e-6) return;
    const double ux = dx/len, uy = dy/len;
    const double px = -uy, py = ux;
    constexpr double ah = 9.0, aw = 4.5;
    const double bx = x2 - ux*ah, by = y2 - uy*ah;
    s << "<polygon points=\""
      << rx(x2) << "," << rx(y2) << " "
      << rx(bx + px*aw) << "," << rx(by + py*aw) << " "
      << rx(bx - px*aw) << "," << rx(by - py*aw)
      << "\" fill=\"" << color << "\"/>\n";
}

}  // namespace

std::string paint_cfg_svg(const Cfg& cfg, const CfgLayout& L) {
    std::ostringstream s;
    s << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << rx(L.width)
      << "\" height=\"" << rx(L.height) << "\" viewBox=\"0 0 " << rx(L.width)
      << " " << rx(L.height)
      << "\" font-family=\"ui-monospace,Menlo,Consolas,monospace\" font-size=\"11\">\n";
    s << "<rect x=\"0\" y=\"0\" width=\"" << rx(L.width)
      << "\" height=\"" << rx(L.height) << "\" fill=\"#ffffff\"/>\n";

    s << "<g>\n";
    s << "<rect x=\"" << rx(L.func_x) << "\" y=\"" << rx(L.func_y)
      << "\" width=\"" << rx(L.func_w) << "\" height=\"" << rx(L.func_h)
      << "\" rx=\"8\" fill=\"#e3f2fd\" stroke=\"#1e88e5\" stroke-width=\"1.4\"/>\n";
    s << "<text x=\"" << rx(L.func_x + L.func_w/2) << "\" y=\"" << rx(L.func_y + 18)
      << "\" text-anchor=\"middle\" fill=\"#0d47a1\" font-weight=\"bold\">function</text>\n";
    s << "<text x=\"" << rx(L.func_x + L.func_w/2) << "\" y=\"" << rx(L.func_y + L.func_h - 12)
      << "\" text-anchor=\"middle\" fill=\"#0d47a1\" font-size=\"12\">"
      << xml_escape(L.func_decl.empty() ? "(no function)" : L.func_decl)
      << "</text>\n";
    s << "</g>\n";

    if (!L.boxes.empty()) {
        const CfgNodeBox& b0 = L.boxes[0];
        const double x1 = L.func_x + L.func_w/2;
        const double y1 = L.func_y + L.func_h;
        const double x2 = b0.cx();
        const double y2 = b0.top();
        s << "<line x1=\"" << rx(x1) << "\" y1=\"" << rx(y1)
          << "\" x2=\"" << rx(x2) << "\" y2=\"" << rx(y2 - 9)
          << "\" stroke=\"#546e7a\" stroke-width=\"1.5\"/>\n";
        emit_arrowhead(s, x1, y1, x2, y2, "#546e7a");
    }

    for (const CfgEdge& e : L.edges) {
        if (e.points.size() < 2) continue;
        const char* col = edge_color(e.kind);
        const bool dashed = (e.kind == EdgeKind::Back);

        s << "<polyline points=\"";
        for (size_t i = 0; i < e.points.size(); ++i) {
            if (i) s << " ";
            s << rx(e.points[i].first) << "," << rx(e.points[i].second);
        }
        s << "\" fill=\"none\" stroke=\"" << col << "\" stroke-width=\"1.5\"";
        if (dashed) s << " stroke-dasharray=\"4 3\"";
        s << "/>\n";

        const auto& a = e.points[e.points.size() - 2];
        const auto& b = e.points.back();
        emit_arrowhead(s, a.first, a.second, b.first, b.second, col);

        if (!e.label.empty() && e.points.size() >= 2) {
            const auto& p0 = e.points[0];
            const auto& p1 = e.points[1];
            const double mx = (p0.first + p1.first) / 2;
            const double my = (p0.second + p1.second) / 2 - 6;
            s << "<text x=\"" << rx(mx) << "\" y=\"" << rx(my)
              << "\" text-anchor=\"middle\" fill=\"" << col
              << "\" font-size=\"10\">" << xml_escape(e.label) << "</text>\n";
        }
    }

    for (size_t i = 0; i < L.boxes.size(); ++i) {
        const CfgNodeBox& b = L.boxes[i];
        const BB& bb = cfg.blocks[i];

        const char* fill;
        const char* stroke;
        const char* text;
        if (i == 0) {
            fill = "#e3f2fd"; stroke = "#1e88e5"; text = "#0d47a1";
        } else {
            fill = "#fafafa"; stroke = "#9e9e9e"; text = "#212121";
        }

        s << "<g>\n";
        s << "<rect x=\"" << rx(b.x) << "\" y=\"" << rx(b.y)
          << "\" width=\"" << rx(b.w) << "\" height=\"" << rx(b.h)
          << "\" rx=\"8\" fill=\"" << fill << "\" stroke=\"" << stroke
          << "\" stroke-width=\"1.2\"/>\n";
        s << "<text x=\"" << rx(b.cx()) << "\" y=\"" << rx(b.y + 15)
          << "\" text-anchor=\"middle\" fill=\"" << text
          << "\" font-weight=\"bold\" font-size=\"10\">BB" << i << "</text>\n";

        const bool has_term = (bb.term.kind != TermKind::Fallthrough);
        double ty = b.y + kHeaderH + 14.0;

        for (const std::string& st : bb.statements) {
            s << "<text x=\"" << rx(b.cx()) << "\" y=\"" << rx(ty)
              << "\" text-anchor=\"middle\" fill=\"" << text << "\">"
              << xml_escape(st) << "</text>\n";
            ty += kLineH;
        }
        if (has_term) {
            s << "<text x=\"" << rx(b.cx()) << "\" y=\"" << rx(ty)
              << "\" text-anchor=\"middle\" fill=\"#5d4037\" font-size=\"10\""
              << " font-style=\"italic\">"
              << xml_escape(terminator_text(bb.term)) << "</text>\n";
            ty += kLineH;
        }
        s << "</g>\n";
    }

    s << "</svg>";
    return s.str();
}

}  // namespace cpp2model