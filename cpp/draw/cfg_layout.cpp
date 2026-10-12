#include "cfg_layout.h"
#include <algorithm>
#include <queue>
#include <functional>

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


std::vector<std::pair<int, std::string>> block_successors(const BB& bb) {
    std::vector<std::pair<int, std::string>> out;
    switch (bb.term.kind) {
        case TermKind::Fallthrough: break;
        case TermKind::Jump:
            if (bb.term.target >= 0) out.emplace_back(bb.term.target, "");
            break;
        case TermKind::Branch:
            if (bb.term.on_true  >= 0) out.emplace_back(bb.term.on_true,  "true");
            if (bb.term.on_false >= 0) out.emplace_back(bb.term.on_false, "false");
            break;
        case TermKind::Switch:
            for (auto& [v, t] : bb.term.cases)
                if (t >= 0) out.emplace_back(t, std::to_string(v));
            if (bb.term.default_target >= 0)
                out.emplace_back(bb.term.default_target, "default");
            break;
        case TermKind::Return: break;
    }
    return out;
}


std::vector<int> compute_depths(const Cfg& cfg,
                                std::vector<CfgEdge>& edges,
                                std::vector<char>& is_back) {
    const int n = static_cast<int>(cfg.blocks.size());
    std::vector<int> depth(n, 0);

    enum { WHITE, GRAY, BLACK };
    std::vector<char> color(n, WHITE);
    is_back.assign(edges.size(), 0);

    std::function<void(int)> dfs = [&](int u) {
        color[u] = GRAY;
        for (size_t ei = 0; ei < edges.size(); ++ei) {
            if (edges[ei].src != u) continue;
            int v = edges[ei].dst;
            if (v < 0 || v >= n) continue;
            if (color[v] == GRAY) {
                is_back[ei] = 1;
            } else if (color[v] == WHITE) {
                dfs(v);
            }
        }
        color[u] = BLACK;
    };
    for (int i = 0; i < n; ++i)
        if (color[i] == WHITE) dfs(i);

    for (int iter = 0; iter < n; ++iter) {
        bool changed = false;
        for (size_t ei = 0; ei < edges.size(); ++ei) {
            if (is_back[ei]) continue;
            int u = edges[ei].src, v = edges[ei].dst;
            if (u < 0 || v < 0 || u >= n || v >= n) continue;
            if (depth[v] < depth[u] + 1) {
                depth[v] = depth[u] + 1;
                changed = true;
            }
        }
        if (!changed) break;
    }
    return depth;
}

}  // namespace

CfgLayout layout_cfg(const Cfg& cfg) {
    CfgLayout L;
    L.func_decl = cfg.func_decl;
    const int n = static_cast<int>(cfg.blocks.size());
    L.boxes.resize(n);

    const std::string func_label =
        cfg.func_decl.empty() ? "(no function)" : cfg.func_decl;
    L.func_w = std::max(kMinW, text_w(func_label) + 2.0 * kPadX);
    L.func_h = kFuncH;

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

        L.boxes[i].block = i;
        L.boxes[i].w = w;
        L.boxes[i].h = kHeaderH + static_cast<double>(lines) * kLineH + kPadY;
    }

    for (int i = 0; i < n; ++i) {
        for (auto& [tgt, label] : block_successors(cfg.blocks[i])) {
            CfgEdge e;
            e.src = i;
            e.dst = tgt;
            e.label = label;
            L.edges.push_back(std::move(e));
        }
    }

    std::vector<char> is_back;
    std::vector<int> depth = compute_depths(cfg, L.edges, is_back);

    for (size_t ei = 0; ei < L.edges.size(); ++ei) {
        CfgEdge& e = L.edges[ei];
        if (is_back[ei]) e.kind = EdgeKind::Back;
        else if (depth[e.dst] > depth[e.src]) e.kind = EdgeKind::Forward;
        else e.kind = EdgeKind::Cross;
    }

    int max_depth = 0;
    for (int i = 0; i < n; ++i) max_depth = std::max(max_depth, depth[i]);
    std::vector<std::vector<int>> layers(max_depth + 1);
    for (int i = 0; i < n; ++i) layers[depth[i]].push_back(i);
    for (int i = 0; i < n; ++i) L.boxes[i].layer = depth[i];
    for (size_t Ly = 0; Ly < layers.size(); ++Ly)
        for (size_t k = 0; k < layers[Ly].size(); ++k)
            L.boxes[layers[Ly][k]].pos_in_layer = static_cast<int>(k);

    std::vector<double> layer_w(layers.size(), 0.0);
    std::vector<double> layer_h(layers.size(), 0.0);
    for (size_t Ly = 0; Ly < layers.size(); ++Ly) {
        double w = 0.0, h = 0.0;
        for (size_t k = 0; k < layers[Ly].size(); ++k) {
            if (k) w += kGapX;
            int i = layers[Ly][k];
            w += L.boxes[i].w;
            h = std::max(h, L.boxes[i].h);
        }
        layer_w[Ly] = w;
        layer_h[Ly] = h;
    }

    double canvas_w = L.func_w;
    for (double w : layer_w) canvas_w = std::max(canvas_w, w);
    canvas_w += 2.0 * kMargin;
    L.width = canvas_w;

    L.func_x = (canvas_w - L.func_w) / 2.0;
    L.func_y = kMargin;
    const double layers_top = L.func_y + L.func_h + kGapY;

    std::vector<double> layer_y(layers.size(), 0.0);
    double y_cursor = layers_top;
    for (size_t Ly = 0; Ly < layers.size(); ++Ly) {
        layer_y[Ly] = y_cursor;
        y_cursor += layer_h[Ly];
        if (Ly + 1 < layers.size()) y_cursor += kLayerGap;
    }
    L.height = y_cursor + kMargin;

    for (size_t Ly = 0; Ly < layers.size(); ++Ly) {
        double x = (canvas_w - layer_w[Ly]) / 2.0;
        for (int i : layers[Ly]) {
            L.boxes[i].x = x;
            L.boxes[i].y = layer_y[Ly] + (layer_h[Ly] - L.boxes[i].h) / 2.0;
            x += L.boxes[i].w + kGapX;
        }
    }

    return L;
}

}  // namespace cpp2model