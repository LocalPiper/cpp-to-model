#include "ast/ast_builder.h"
#include "ast/clang_session.h"
#include "draw/ast_drawer.h"
#include "facade.h"
#include "json_out.h"
#include "text_utils.h"

#include <cctype>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using namespace cpp2model;

namespace {

constexpr size_t kMaxInputBytes = 200000;

}  // namespace

int main() {
    std::ios::sync_with_stdio(false);

    std::string code;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!code.empty()) code += '\n';
        code += line;
    }

    bool blank = true;
    for (unsigned char c : code) {
        if (!std::isspace(c)) { blank = false; break; }
    }
    if (blank) {
        std::cout << "{\"ok\":false,\"error\":\"No input provided.\"}\n";
        return 0;
    }
    if (code.size() > kMaxInputBytes) {
        std::cout << "{\"ok\":false,\"error\":\"Input too large (limit 200000 bytes).\"}\n";
        return 0;
    }

    InputFacade facade(std::make_unique<PassthroughStrategy>());
    std::string source = facade.wrap(code);

    AstBuilder builder;
    ParseSummary summary;
    parse_and_build(source, builder, summary);

    const TreeModel& tree = builder.tree();

    AstDrawer drawer;
    std::string svg = drawer.render(tree);

    std::ostringstream out;
    out << "{\n";
    out << "  \"ok\": " << (summary.errors == 0 ? "true" : "false") << ",\n";
    out << "  \"node_count\": " << (static_cast<int>(tree.nodes.size()) - 1) << ",\n";
    out << "  \"truncated\": " << (tree.truncated ? "true" : "false") << ",\n";
    out << "  \"errors\": " << summary.errors << ",\n";
    out << "  \"warnings\": " << summary.warnings << ",\n";
    out << "  \"diagnostics\": [";
    for (size_t i = 0; i < summary.diagnostics.size(); ++i) {
        if (i) out << ",";
        out << "\"" << escape_json(summary.diagnostics[i]) << "\"";
    }
    out << "],\n";
    out << "  \"ast\": ";
    emit_tree_json(out, tree);
    out << ",\n";
    out << "  \"svg\": \"" << escape_json(svg) << "\"\n";
    out << "}\n";

    std::cout << out.str();
    return 0;
}