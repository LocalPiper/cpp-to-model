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

constexpr int kMaxNodes = 400;
constexpr int kMaxDepth = 40;

std::string int_to_string(const llvm::APInt& v, bool isSigned) {
    llvm::SmallString<32> buf;
    v.toString(buf, 10, isSigned);
    return buf.str().str();
}

std::string apsint_to_string(const llvm::APSInt& v) {
    llvm::SmallString<32> buf;
    v.toString(buf, 10);
    return buf.str().str();
}

const char* trait_name(clang::UnaryExprOrTypeTrait k) {
    switch (k) {
        case clang::UETT_SizeOf:                return "sizeof";
        case clang::UETT_AlignOf:               return "alignof";
        case clang::UETT_PreferredAlignOf:      return "alignof (preferred)";
        case clang::UETT_VecStep:               return "vec_step";
        case clang::UETT_OpenMPRequiredSimdAlign: return "omp_simd_align";
        default:                                return "trait";
    }
}

std::string loc_str(const clang::SourceManager* sm, clang::SourceLocation loc) {
    if (loc.isInvalid() || !sm) return "";
    clang::PresumedLoc pl = sm->getPresumedLoc(loc);
    if (pl.isInvalid()) return "";
    std::ostringstream os;
    os << "L" << pl.getLine() << ":" << pl.getColumn();
    return os.str();
}

std::string range_str(const clang::SourceManager* sm, clang::SourceRange r) {
    std::string b = loc_str(sm, r.getBegin());
    std::string e = loc_str(sm, r.getEnd());
    if (b.empty()) return "";
    if (e.empty() || b == e) return b;
    return b + "-" + e;
}

}  // namespace

AstBuilder::AstBuilder() {
    Node root;
    root.cat = NodeCategory::Root;
    root.kind = "TranslationUnit";
    root.detail = "input.cc";
    m_tree.nodes.push_back(std::move(root));
    m_current = 0;
}

void AstBuilder::build(clang::ASTContext& ctx) {
    m_sm = &ctx.getSourceManager();
    for (auto* d : ctx.getTranslationUnitDecl()->decls()) walkDecl(d);
}

void AstBuilder::add(NodeCategory cat, const std::string& kind,
                     const std::string& detail, const std::string& type,
                     const std::string& range) {
    Node n;
    n.cat = cat;
    n.kind = kind;
    n.detail = display_escape(truncate(detail, 60));
    n.type = truncate(type, 60);
    n.range = range;
    m_tree.nodes[m_current].children.push_back(static_cast<int>(m_tree.nodes.size()));
    m_tree.nodes.push_back(std::move(n));
}

bool AstBuilder::limit_hit() {
    if (m_depth >= kMaxDepth || static_cast<int>(m_tree.nodes.size()) >= kMaxNodes) {
        m_tree.truncated = true;
        return true;
    }
    return false;
}


std::string AstBuilder::name_of(const clang::Decl* d) const {
    if (auto* nd = clang::dyn_cast<clang::NamedDecl>(d))
        return nd->getDeclName().getAsString();
    return "";
}

std::string AstBuilder::decl_type(const clang::Decl* d) const {
    if (auto* fd = clang::dyn_cast<clang::FunctionDecl>(d))
        return fd->getReturnType().getAsString();
    if (auto* vd = clang::dyn_cast<clang::ValueDecl>(d))
        return vd->getType().getAsString();
    if (auto* td = clang::dyn_cast<clang::TypedefNameDecl>(d))
        return td->getUnderlyingType().getAsString();
    return "";
}

std::string AstBuilder::decl_detail(const clang::Decl* d) const {
    std::string n = name_of(d);
    if (!n.empty()) return n;
    if (clang::isa<clang::NamespaceDecl>(d)) return "(anonymous)";
    if (auto* ec = clang::dyn_cast<clang::EnumConstantDecl>(d))
        return apsint_to_string(ec->getInitVal());
    return "";
}

std::string AstBuilder::stmt_detail(clang::Stmt* s) const {
    if (auto* bo = clang::dyn_cast<clang::BinaryOperator>(s))
        return bo->getOpcodeStr().str();
    if (auto* uo = clang::dyn_cast<clang::UnaryOperator>(s))
        return clang::UnaryOperator::getOpcodeStr(uo->getOpcode()).str();
    if (auto* il = clang::dyn_cast<clang::IntegerLiteral>(s))
        return truncate(int_to_string(il->getValue(),
                                      il->getType()->isSignedIntegerType()), 24);
    if (auto* fl = clang::dyn_cast<clang::FloatingLiteral>(s)) {
        std::ostringstream os;
        os << std::setprecision(6) << fl->getValue().convertToDouble();
        return os.str();
    }
    if (auto* sl = clang::dyn_cast<clang::StringLiteral>(s)) {
        std::string v(sl->getBytes().data(), sl->getBytes().size());
        return truncate(display_escape(v), 28);
    }
    if (auto* chl = clang::dyn_cast<clang::CharacterLiteral>(s)) {
        std::ostringstream os;
        os << "'" << static_cast<char>(chl->getValue()) << "'";
        return os.str();
    }
    if (auto* bl = clang::dyn_cast<clang::CXXBoolLiteralExpr>(s))
        return bl->getValue() ? "true" : "false";
    if (clang::isa<clang::CXXNullPtrLiteralExpr>(s))
        return "nullptr";
    if (auto* dr = clang::dyn_cast<clang::DeclRefExpr>(s))
        return name_of(dr->getDecl());
    if (auto* me = clang::dyn_cast<clang::MemberExpr>(s)) {
        std::string n = me->getMemberDecl() ? name_of(me->getMemberDecl()) : "";
        return n.empty() ? "" : "." + n;
    }
    if (auto* ce = clang::dyn_cast<clang::CallExpr>(s)) {
        if (const clang::FunctionDecl* fd = ce->getDirectCallee())
            return name_of(fd);
        if (auto* dre = clang::dyn_cast<clang::DeclRefExpr>(ce->getCallee()))
            return name_of(dre->getDecl());
        return "";
    }
    if (auto* coe = clang::dyn_cast<clang::CXXOperatorCallExpr>(s)) {
        const char* op = clang::getOperatorSpelling(coe->getOperator());
        return op ? op : "";
    }
    if (auto* cx = clang::dyn_cast<clang::CXXConstructExpr>(s)) {
        if (const clang::CXXConstructorDecl* cd = cx->getConstructor())
            return name_of(cd);
        return "";
    }
    if (auto* cast = clang::dyn_cast<clang::CastExpr>(s)) {
        const char* cn = clang::CastExpr::getCastKindName(cast->getCastKind());
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
    if (auto* sz = clang::dyn_cast<clang::UnaryExprOrTypeTraitExpr>(s))
        return trait_name(sz->getKind());
    if (auto* ds = clang::dyn_cast<clang::DeclStmt>(s)) {
        std::string out;
        for (auto* d : ds->decls()) {
            if (!out.empty()) out += ", ";
            if (auto* vd = clang::dyn_cast<clang::VarDecl>(d))
                out += vd->getType().getAsString() + " " + name_of(vd);
            else
                out += name_of(d);
        }
        return truncate(out, 40);
    }
    if (auto* cmp = clang::dyn_cast<clang::CXXRewrittenBinaryOperator>(s))
        return clang::BinaryOperator::getOpcodeStr(cmp->getOperator()).str();
    return "";
}

std::string AstBuilder::stmt_type(clang::Stmt* s) const {
    if (auto* e = clang::dyn_cast<clang::Expr>(s))
        return e->getType().getAsString();
    return "";
}

bool AstBuilder::is_transparent(clang::Stmt* s) const {
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

void AstBuilder::walkDecl(clang::Decl* d) {
    if (!d) return;
    if (d->isImplicit()) return;
    if (limit_hit()) return;

    int parent = m_current;
    add(NodeCategory::Declaration, std::string(d->getDeclKindName()), decl_detail(d),
        decl_type(d), range_str(m_sm, d->getSourceRange()));
    m_current = static_cast<int>(m_tree.nodes.size()) - 1;
    ++m_depth;

    if (auto* fd = clang::dyn_cast<clang::FunctionDecl>(d)) {
        for (auto* p : fd->parameters()) walkDecl(p);
        if (fd->getBody()) walkStmt(fd->getBody());
    } else if (auto* dc = clang::dyn_cast<clang::DeclContext>(d)) {
        for (auto* child : dc->decls()) walkDecl(child);
    }
    if (auto* vd = clang::dyn_cast<clang::VarDecl>(d)) {
        if (vd->hasInit()) walkStmt(vd->getInit());
    } else if (auto* sa = clang::dyn_cast<clang::StaticAssertDecl>(d)) {
        walkStmt(sa->getAssertExpr());
    }

    m_current = parent;
    --m_depth;
}

void AstBuilder::walkStmt(clang::Stmt* s) {
    if (!s) return;
    if (is_transparent(s)) {
        for (auto* child : s->children()) walkStmt(child);
        return;
    }
    if (limit_hit()) return;

    int parent = m_current;
    NodeCategory cat =
        clang::isa<clang::Expr>(s) ? NodeCategory::Expression : NodeCategory::Statement;
    add(cat, std::string(s->getStmtClassName()), stmt_detail(s),
        stmt_type(s), range_str(m_sm, s->getSourceRange()));
    m_current = static_cast<int>(m_tree.nodes.size()) - 1;
    ++m_depth;

    if (auto* ds = clang::dyn_cast<clang::DeclStmt>(s)) {
        for (auto* d : ds->decls()) walkDecl(d);
    } else {
        for (auto* child : s->children()) walkStmt(child);
    }

    m_current = parent;
    --m_depth;
}

namespace {

class AstCollector : public clang::ASTConsumer {
public:
    explicit AstCollector(AstBuilder& b) : m_b(b) {}
    void HandleTranslationUnit(clang::ASTContext& ctx) override { m_b.build(ctx); }
private:
    AstBuilder& m_b;
};

class AstDumpAction : public clang::ASTFrontendAction {
public:
    explicit AstDumpAction(AstBuilder& b) : m_b(b) {}
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
        clang::CompilerInstance&, llvm::StringRef) override {
        return std::make_unique<AstCollector>(m_b);
    }
private:
    AstBuilder& m_b;
};

}  // namespace

bool parse_and_build(const std::string& code, AstBuilder& builder, ParseSummary& out) {
    auto action = std::make_unique<AstDumpAction>(builder);
    return run_tool_on_code(code, std::move(action), out);
}

}  // namespace cpp2model