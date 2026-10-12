#include "ast_painter.h"
#include <sstream>
#include <cmath>
#include "../text_utils.h"

namespace cpp2model {
namespace {

constexpr int kNodeH = 34;

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

long long rx(double v) { return static_cast<long long>(std::llround(v)); }

}  // namespace

std::string paint_ast_svg(const TreeModel& tree, const AstLayout& L) {
    std::ostringstream s;
    s << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << rx(L.width)
      << "\" height=\"" << rx(L.height) << "\" viewBox=\"0 0 " << rx(L.width)
      << " " << rx(L.height)
      << "\" font-family=\"ui-monospace,Menlo,Consolas,monospace\" font-size=\"11\">\n";
    s << "<rect x=\"0\" y=\"0\" width=\"" << rx(L.width)
      << "\" height=\"" << rx(L.height) << "\" fill=\"#ffffff\"/>\n";

    // Edges.
    for (size_t i = 1; i < L.boxes.size(); ++i) {
        const AstNodeBox& b = L.boxes[i];
        if (b.parent < 0) continue;
        const AstNodeBox& p = L.boxes[b.parent];
        s << "<line x1=\"" << rx(p.cx) << "\" y1=\"" << rx(p.y + kNodeH)
          << "\" x2=\"" << rx(b.cx) << "\" y2=\"" << rx(b.y)
          << "\" stroke=\"#90a4ae\" stroke-width=\"1.5\"/>\n";
    }

    // Nodes.
    for (size_t i = 0; i < L.boxes.size(); ++i) {
        const Node& n = tree.nodes[i];
        const AstNodeBox& b = L.boxes[i];

        std::string title = n.kind;
        if (!n.detail.empty()) title += " | " + n.detail;
        if (!n.type.empty())   title += " | " + n.type;
        if (!n.range.empty())  title += " | " + n.range;

        const double nx = b.cx - b.w / 2.0;

        s << "<g title=\"" << xml_escape(title) << "\">\n";
        s << "<rect x=\"" << rx(nx) << "\" y=\"" << rx(b.y)
          << "\" width=\"" << rx(b.w) << "\" height=\"" << kNodeH
          << "\" rx=\"6\" fill=\"" << fill_for(n.cat)
          << "\" stroke=\"" << stroke_for(n.cat) << "\" stroke-width=\"1.2\"/>\n";
        s << "<text x=\"" << rx(b.cx) << "\" y=\"" << rx(b.y + 14)
          << "\" text-anchor=\"middle\" fill=\"" << text_for(n.cat)
          << "\" font-weight=\"bold\">" << xml_escape(n.kind) << "</text>\n";
        if (!n.detail.empty()) {
            s << "<text x=\"" << rx(b.cx) << "\" y=\"" << rx(b.y + 27)
              << "\" text-anchor=\"middle\" fill=\"" << text_for(n.cat) << "\">"
              << xml_escape(n.detail) << "</text>\n";
        }
        s << "</g>\n";
    }

    if (tree.truncated) {
        s << "<text x=\"30\" y=\"" << rx(L.height - 10)
          << "\" fill=\"#b71c1c\" font-size=\"12\">"
          << "... AST truncated (node limit reached)</text>\n";
    }
    s << "</svg>";
    return s.str();
}

}  // namespace cpp2model