#include "ast_builder.h"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/Stmt.h>
#include <clang/AST/Type.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/APSInt.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>

#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "../text_utils.h"

namespace cpp2model {
namespace {

constexpr int kMaxNodes = 800;
constexpr int kMaxDepth = 80;

std::string intToString(const llvm::APInt &v, bool isSigned) {
  llvm::SmallString<32> buf;
  v.toString(buf, 10, isSigned);
  return buf.str().str();
}

std::string apsintToString(const llvm::APSInt &v) {
  llvm::SmallString<32> buf;
  v.toString(buf, 10);
  return buf.str().str();
}

const char *traitName(clang::UnaryExprOrTypeTrait k) {
  switch (k) {
  case clang::UETT_SizeOf:
    return "sizeof";
  case clang::UETT_AlignOf:
    return "alignof";
  case clang::UETT_PreferredAlignOf:
    return "alignof (preferred)";
  case clang::UETT_VecStep:
    return "vec_step";
  case clang::UETT_OpenMPRequiredSimdAlign:
    return "omp_simd_align";
  default:
    return "trait";
  }
}

std::string locStr(const clang::SourceManager *sm, clang::SourceLocation loc) {
  if (loc.isInvalid() || !sm)
    return "";
  clang::PresumedLoc pl = sm->getPresumedLoc(loc);
  if (pl.isInvalid())
    return "";
  std::ostringstream os;
  os << "L" << pl.getLine() << ":" << pl.getColumn();
  return os.str();
}

std::string rangeStr(const clang::SourceManager *sm, clang::SourceRange r) {
  std::string b = locStr(sm, r.getBegin());
  std::string e = locStr(sm, r.getEnd());
  if (b.empty())
    return "";
  if (e.empty() || b == e)
    return b;
  return b + "-" + e;
}

} // namespace

AstBuilder::AstBuilder() {
  Node root;
  root.cat = NodeCategory::Root;
  root.kind = "TranslationUnit";
  root.detail = "input.cc";
  Tree.nodes.push_back(std::move(root));
  Current = 0;
}

void AstBuilder::build(clang::ASTContext &ctx) {
  SM = &ctx.getSourceManager();
  Context = &ctx;
  for (auto *d : ctx.getTranslationUnitDecl()->decls())
    walkDecl(d);
}

int AstBuilder::add(NodeCategory cat, const std::string &kind,
                    const std::string &detail, const std::string &type,
                    const std::string &range) {
  Node n;
  n.cat = cat;
  n.kind = kind;
  n.detail = displayEscape(truncate(detail, 60));
  n.type = truncate(type, 60);
  n.range = range;
  int idx = static_cast<int>(Tree.nodes.size());
  Tree.nodes[Current].children.push_back(idx);
  Tree.nodes.push_back(std::move(n));
  return idx;
}

bool AstBuilder::limitHit() {
  if (Depth >= kMaxDepth || static_cast<int>(Tree.nodes.size()) >= kMaxNodes) {
    Tree.truncated = true;
    return true;
  }
  return false;
}

std::string AstBuilder::nameOf(const clang::Decl *d) const {
  if (auto *nd = clang::dyn_cast<clang::NamedDecl>(d))
    return nd->getDeclName().getAsString();
  return "";
}

std::string AstBuilder::declType(const clang::Decl *d) const {
  if (auto *fd = clang::dyn_cast<clang::FunctionDecl>(d))
    return fd->getReturnType().getAsString();
  if (auto *vd = clang::dyn_cast<clang::ValueDecl>(d))
    return vd->getType().getAsString();
  if (auto *td = clang::dyn_cast<clang::TypedefNameDecl>(d))
    return td->getUnderlyingType().getAsString();
  return "";
}

std::string AstBuilder::declDetail(const clang::Decl *d) const {
  std::string n = nameOf(d);
  if (!n.empty())
    return n;
  if (clang::isa<clang::NamespaceDecl>(d))
    return "(anonymous)";
  if (auto *ec = clang::dyn_cast<clang::EnumConstantDecl>(d))
    return apsintToString(ec->getInitVal());
  return "";
}

std::string AstBuilder::stmtDetail(clang::Stmt *s) const {
  if (auto *bo = clang::dyn_cast<clang::BinaryOperator>(s))
    return bo->getOpcodeStr().str();
  if (auto *uo = clang::dyn_cast<clang::UnaryOperator>(s))
    return clang::UnaryOperator::getOpcodeStr(uo->getOpcode()).str();
  if (auto *il = clang::dyn_cast<clang::IntegerLiteral>(s))
    return truncate(
        intToString(il->getValue(), il->getType()->isSignedIntegerType()), 24);
  if (auto *fl = clang::dyn_cast<clang::FloatingLiteral>(s)) {
    std::ostringstream os;
    os << std::setprecision(6) << fl->getValue().convertToDouble();
    return os.str();
  }
  if (auto *sl = clang::dyn_cast<clang::StringLiteral>(s)) {
    std::string v(sl->getBytes().data(), sl->getBytes().size());
    return truncate(displayEscape(v), 28);
  }
  if (auto *chl = clang::dyn_cast<clang::CharacterLiteral>(s)) {
    std::ostringstream os;
    os << "'" << static_cast<char>(chl->getValue()) << "'";
    return os.str();
  }
  if (auto *bl = clang::dyn_cast<clang::CXXBoolLiteralExpr>(s))
    return bl->getValue() ? "true" : "false";
  if (clang::isa<clang::CXXNullPtrLiteralExpr>(s))
    return "nullptr";
  if (auto *dr = clang::dyn_cast<clang::DeclRefExpr>(s))
    return nameOf(dr->getDecl());
  if (auto *me = clang::dyn_cast<clang::MemberExpr>(s)) {
    std::string n = me->getMemberDecl() ? nameOf(me->getMemberDecl()) : "";
    return n.empty() ? "" : "." + n;
  }
  if (auto *ce = clang::dyn_cast<clang::CallExpr>(s)) {
    if (const clang::FunctionDecl *fd = ce->getDirectCallee())
      return nameOf(fd);
    if (auto *dre = clang::dyn_cast<clang::DeclRefExpr>(ce->getCallee()))
      return nameOf(dre->getDecl());
    return "";
  }
  if (auto *coe = clang::dyn_cast<clang::CXXOperatorCallExpr>(s)) {
    const char *op = clang::getOperatorSpelling(coe->getOperator());
    return op ? op : "";
  }
  if (auto *cx = clang::dyn_cast<clang::CXXConstructExpr>(s)) {
    if (const clang::CXXConstructorDecl *cd = cx->getConstructor())
      return nameOf(cd);
    return "";
  }
  if (auto *cast = clang::dyn_cast<clang::CastExpr>(s)) {
    const char *cn = clang::CastExpr::getCastKindName(cast->getCastKind());
    if (clang::isa<clang::CStyleCastExpr>(s))
      return std::string("C-style ") + (cn ? cn : "");
    if (clang::isa<clang::CXXStaticCastExpr>(s))
      return std::string("static_cast ") + (cn ? cn : "");
    if (clang::isa<clang::CXXDynamicCastExpr>(s))
      return std::string("dynamic_cast ") + (cn ? cn : "");
    if (clang::isa<clang::CXXReinterpretCastExpr>(s))
      return std::string("reinterpret_cast ") + (cn ? cn : "");
    if (clang::isa<clang::CXXConstCastExpr>(s))
      return std::string("const_cast ") + (cn ? cn : "");
    return cn ? cn : "";
  }
  if (auto *gs = clang::dyn_cast<clang::GotoStmt>(s))
    return nameOf(gs->getLabel());
  if (auto *ls = clang::dyn_cast<clang::LabelStmt>(s))
    return nameOf(ls->getDecl());
  if (auto *cs = clang::dyn_cast<clang::CaseStmt>(s)) {
    if (Context) {
      clang::Expr::EvalResult result;
      if (cs->getLHS()->EvaluateAsInt(result, *Context)) {
        llvm::APSInt v = result.Val.getInt();
        llvm::SmallString<32> buf;
        v.toString(buf, 10, v.isSigned());
        return buf.str().str();
      }
    }
    return "?";
  }
  if (clang::isa<clang::DefaultStmt>(s))
    return "default";
  if (auto *sz = clang::dyn_cast<clang::UnaryExprOrTypeTraitExpr>(s))
    return traitName(sz->getKind());
  if (auto *ds = clang::dyn_cast<clang::DeclStmt>(s)) {
    std::string out;
    for (auto *d : ds->decls()) {
      if (!out.empty())
        out += ", ";
      if (auto *vd = clang::dyn_cast<clang::VarDecl>(d))
        out += vd->getType().getAsString() + " " + nameOf(vd);
      else
        out += nameOf(d);
    }
    return truncate(out, 40);
  }
  if (auto *cmp = clang::dyn_cast<clang::CXXRewrittenBinaryOperator>(s))
    return clang::BinaryOperator::getOpcodeStr(cmp->getOperator()).str();
  return "";
}

std::string AstBuilder::stmtType(clang::Stmt *s) const {
  if (auto *e = clang::dyn_cast<clang::Expr>(s))
    return e->getType().getAsString();
  return "";
}

bool AstBuilder::isTransparent(clang::Stmt *s) const {
  return clang::isa<clang::ImplicitCastExpr>(s) ||
         clang::isa<clang::CXXBindTemporaryExpr>(s) ||
         clang::isa<clang::MaterializeTemporaryExpr>(s) ||
         clang::isa<clang::ExprWithCleanups>(s) ||
         clang::isa<clang::ConstantExpr>(s) ||
         clang::isa<clang::CXXDefaultArgExpr>(s) ||
         clang::isa<clang::CXXDefaultInitExpr>(s) ||
         clang::isa<clang::SubstNonTypeTemplateParmExpr>(s) ||
         clang::isa<clang::OpaqueValueExpr>(s);
}

void AstBuilder::walkPlaceholderOr(clang::Stmt *s, const char *kind) {
  if (s) {
    walkStmt(s);
    return;
  }
  add(NodeCategory::Statement, kind, "", "", "");
}

void AstBuilder::walkRangeDecl(clang::DeclStmt *ds) {
  if (!ds)
    return;
  for (auto *d : ds->decls()) {
    auto *vd = clang::dyn_cast<clang::VarDecl>(d);
    if (!vd)
      continue;

    int parent = Current;
    int idx = add(NodeCategory::Declaration, "Var", nameOf(vd), declType(vd),
                  rangeStr(SM, vd->getSourceRange()));
    Current = idx;
    ++Depth;
    if (vd->hasInit())
      walkStmt(vd->getInit());
    --Depth;
    Current = parent;
  }
}

void AstBuilder::walkDecl(clang::Decl *d) {
  if (!d)
    return;
  if (d->isImplicit())
    return;
  if (limitHit())
    return;

  int parent = Current;
  int idx = add(NodeCategory::Declaration, std::string(d->getDeclKindName()),
                declDetail(d), declType(d), rangeStr(SM, d->getSourceRange()));

  Current = idx;
  ++Depth;

  if (auto *fd = clang::dyn_cast<clang::FunctionDecl>(d)) {
    for (auto *p : fd->parameters())
      walkDecl(p);
    if (fd->getBody())
      walkStmt(fd->getBody());
  } else if (auto *dc = clang::dyn_cast<clang::DeclContext>(d)) {
    for (auto *child : dc->decls())
      walkDecl(child);
  }
  if (auto *vd = clang::dyn_cast<clang::VarDecl>(d)) {
    if (vd->hasInit())
      walkStmt(vd->getInit());
  } else if (auto *sa = clang::dyn_cast<clang::StaticAssertDecl>(d)) {
    walkStmt(sa->getAssertExpr());
  }

  Current = parent;
  --Depth;
}

void AstBuilder::walkStmt(clang::Stmt *s) {
  if (!s)
    return;
  if (isTransparent(s)) {
    for (auto *child : s->children())
      walkStmt(child);
    return;
  }
  if (limitHit())
    return;

  int parent = Current;
  NodeCategory cat = clang::isa<clang::Expr>(s) ? NodeCategory::Expression
                                                : NodeCategory::Statement;
  int idx = add(cat, std::string(s->getStmtClassName()), stmtDetail(s),
                stmtType(s), rangeStr(SM, s->getSourceRange()));

  Current = idx;
  ++Depth;

  if (auto *ds = clang::dyn_cast<clang::DeclStmt>(s)) {
    for (auto *d : ds->decls())
      walkDecl(d);
  } else if (auto *ifs = clang::dyn_cast<clang::IfStmt>(s)) {
    walkPlaceholderOr(ifs->getInit(), "EmptyInit");
    walkStmt(ifs->getCond());
    walkStmt(ifs->getThen());
    walkPlaceholderOr(ifs->getElse(), "EmptyElse");
  } else if (auto *ws = clang::dyn_cast<clang::WhileStmt>(s)) {
    walkPlaceholderOr(ws->getConditionVariableDeclStmt(), "NoConditionVar");
    walkStmt(ws->getCond());
    walkStmt(ws->getBody());
  } else if (auto *fs = clang::dyn_cast<clang::ForStmt>(s)) {
    walkPlaceholderOr(fs->getInit(), "EmptyInit");
    walkPlaceholderOr(fs->getCond(), "EmptyCond");
    walkPlaceholderOr(fs->getInc(), "EmptyInc");
    walkStmt(fs->getBody());
  } else if (auto *ss = clang::dyn_cast<clang::SwitchStmt>(s)) {
    walkPlaceholderOr(ss->getInit(), "EmptyInit");
    walkStmt(ss->getCond());
    walkStmt(ss->getBody());
  } else if (auto *frs = clang::dyn_cast<clang::CXXForRangeStmt>(s)) {
    walkRangeDecl(frs->getRangeStmt());
    walkRangeDecl(frs->getBeginStmt());
    walkRangeDecl(frs->getEndStmt());
    walkStmt(frs->getCond());
    walkStmt(frs->getInc());
    walkRangeDecl(frs->getLoopVarStmt());
    walkStmt(frs->getBody());
  } else {
    for (auto *child : s->children())
      walkStmt(child);
  }

  Current = parent;
  --Depth;
}

namespace {

class AstCollector : public clang::ASTConsumer {
public:
  explicit AstCollector(AstBuilder &b) : Builder(b) {}
  void HandleTranslationUnit(clang::ASTContext &ctx) override {
    Builder.build(ctx);
  }

private:
  AstBuilder &Builder;
};

class AstDumpAction : public clang::ASTFrontendAction {
public:
  explicit AstDumpAction(AstBuilder &b) : Builder(b) {}
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &, llvm::StringRef) override {
    return std::make_unique<AstCollector>(Builder);
  }

private:
  AstBuilder &Builder;
};

} // namespace

bool parseAndBuild(const std::string &code, AstBuilder &builder,
                   ParseSummary &out) {
  auto action = std::make_unique<AstDumpAction>(builder);
  return runToolOnCode(code, std::move(action), out);
}

} // namespace cpp2model