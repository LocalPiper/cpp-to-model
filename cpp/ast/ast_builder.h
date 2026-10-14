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
class DeclStmt;
} // namespace clang

namespace cpp2model {

class AstBuilder {
public:
  AstBuilder();

  void build(clang::ASTContext &ctx);

  const TreeModel &tree() const { return Tree; }
  bool truncated() const { return Tree.truncated; }

private:
  TreeModel Tree;
  clang::SourceManager *SM = nullptr;
  clang::ASTContext *Context = nullptr;
  int Current = 0;
  int Depth = 0;

  int add(NodeCategory cat, const std::string &kind, const std::string &detail,
          const std::string &type, const std::string &range);
  bool limitHit();

  std::string nameOf(const clang::Decl *d) const;
  std::string declType(const clang::Decl *d) const;
  std::string declDetail(const clang::Decl *d) const;
  std::string stmtDetail(clang::Stmt *s) const;
  std::string stmtType(clang::Stmt *s) const;
  bool isTransparent(clang::Stmt *s) const;
  void walkPlaceholderOr(clang::Stmt *s, const char *kind);
  void walkRangeDecl(clang::DeclStmt *ds);

  void walkDecl(clang::Decl *d);
  void walkStmt(clang::Stmt *s);
};

bool parseAndBuild(const std::string &code, AstBuilder &builder,
                   ParseSummary &out);

} // namespace cpp2model