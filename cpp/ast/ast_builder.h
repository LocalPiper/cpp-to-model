#pragma once

#include <memory>
#include <string>

#include "../model.h"
#include "clang_session.h"

namespace clang {
class ASTContext;
class Decl;
class SourceManager;
class Stmt;
}  // namespace clang

namespace cpp2model {

class AstBuilder {
public:
    AstBuilder();

    void build(clang::ASTContext& ctx);

    const TreeModel& tree() const { return m_tree; }
    bool truncated() const { return m_tree.truncated; }

private:
    TreeModel m_tree;
    clang::SourceManager* m_sm = nullptr;
    int m_current = 0;
    int m_depth = 0;

    void add(NodeCategory cat, const std::string& kind, const std::string& detail,
             const std::string& type, const std::string& range);
    bool limit_hit();

    std::string name_of(const clang::Decl* d) const;
    std::string decl_type(const clang::Decl* d) const;
    std::string decl_detail(const clang::Decl* d) const;
    std::string stmt_detail(clang::Stmt* s) const;
    std::string stmt_type(clang::Stmt* s) const;
    bool is_transparent(clang::Stmt* s) const;

    void walkDecl(clang::Decl* d);
    void walkStmt(clang::Stmt* s);
};


bool parse_and_build(const std::string& code, AstBuilder& builder, ParseSummary& out);

}  // namespace cpp2model