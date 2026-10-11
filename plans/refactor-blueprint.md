# Refactor blueprint: split `cpp/processor.cpp`, abstract the drawer, and a clang walker guide

## 1. Why split

`cpp/processor.cpp` is 766 lines holding nine distinct responsibilities:

| # | Responsibility | Where it lives today |
|---|----------------|----------------------|
| 1 | stdin reading + validation + JSON envelope | `main()` |
| 2 | text escaping (JSON / SVG display / XML attributes) | text helpers |
| 3 | LLVM 21 compat shims (APInt/APSInt printing, trait names) | text helpers |
| 4 | generic tree model + `AstBuilder` walker | `Node`, `AstBuilder` |
| 5 | SVG rendering with custom tidy-tree layout | `render_svg` + color maps |
| 6 | JSON tree serialization | `emit_node_json` |
| 7 | clang diagnostic collection | `CollectingDiagConsumer` |
| 8 | clang FrontendAction / ASTConsumer wiring | `AstCollector`, `AstDumpAction` |
| 9 | in-memory parse driver | `run_on_code` |

Splitting gives: short readable files, explicit seams for the three pillars, a
reusable parse engine for future walkers, and an extensible drawer family.

## 2. Target architecture — the three pillars

```
raw input ──▶ InputFacade ──▶ cpp source text ──▶ clang session (parse)
                                                     │
                                                     ▼
                                              AstBuilder walker
                                                     │
                                                     ▼
                                              TreeModel (generic nodes)
                                               /            \
                                    AstDrawer (SVG)      json serializer
```

- **Pillar A — InputFacade**: boundary that takes raw user input and produces a
  complete, compilable C++ translation unit. Future strategies: passthrough
  (today), wrap-in-main, expression mode, snippet-with-includes.
- **Pillar B — AstBuilder**: the clang AST walker. Takes `ASTContext`, produces a
  generic `TreeModel`. Nothing downstream may include clang headers.
- **Pillar C — Drawer**: abstract base, knows nothing about clang or AST.
  `AstDrawer` is the first concrete implementation (TreeModel → SVG). Future
  concrete drawers (CFG, call graph, data flow, ...) implement the same interface.

## 3. Proposed file layout

```text
cpp/
  main.cpp                     # composition root: stdin → facade → parse → walk → draw/json → stdout
  facade.h / facade.cpp        # InputFacade + WrappingStrategy, default passthrough
  model.h                      # Node, NodeCategory, TreeModel — generic, clang-free
  text_utils.h / text_utils.cpp  # escape_json, display_escape, xml_escape, truncate
  json_out.h / json_out.cpp    # TreeModel → JSON (recursive emitter)
  ast/
    ast_builder.h / ast_builder.cpp     # AstBuilder walker + AstCollector/AstDumpAction
    clang_session.h / clang_session.cpp # CollectingDiagConsumer, run_tool_on_code
  draw/
    drawer.h                   # abstract Drawer interface
    ast_drawer.h / ast_drawer.cpp       # AstDrawer: tidy-tree layout + category styling → SVG
```

## 4. Key interfaces

```cpp
// facade.h
class WrappingStrategy {
public:
    virtual ~WrappingStrategy() = default;
    virtual std::string wrap(std::string_view raw) const = 0;
};
class PassthroughStrategy : public WrappingStrategy {
public:
    std::string wrap(std::string_view raw) const override;
};
class InputFacade {
public:
    explicit InputFacade(std::unique_ptr<WrappingStrategy> s);
    std::string wrap(std::string_view raw) const;   // raw input -> full cpp source
private:
    std::unique_ptr<WrappingStrategy> m_strategy;
};

// model.h — generic drawable tree, NO clang types
enum class NodeCategory { Root, Declaration, Statement, Expression };
struct Node {
    NodeCategory cat;
    std::string kind;
    std::string detail;
    std::string type;
    std::string range;
    std::vector<int> children;   // indices into TreeModel::nodes
};
struct TreeModel {
    std::vector<Node> nodes;
    bool truncated = false;
};

// draw/drawer.h — abstract, clang-free
class Drawer {
public:
    virtual ~Drawer() = default;
    virtual std::string render(const TreeModel& tree) const = 0;
    virtual const char* format() const = 0;   // e.g. "svg"
};

// draw/ast_drawer.h — first concrete drawer
class AstDrawer final : public Drawer {
public:
    std::string render(const TreeModel& tree) const override;
    const char* format() const override { return "svg"; }
};

// ast/ast_builder.h — the walker
class AstBuilder {
public:
    AstBuilder();
    void build(clang::ASTContext& ctx);   // entry point from ASTConsumer
    const TreeModel& tree() const;
    bool truncated() const;
};

// ast/clang_session.h — reusable parse engine for any walker
struct ParseSummary {
    std::vector<std::string> diagnostics;
    unsigned errors = 0;
    unsigned warnings = 0;
};
bool run_tool_on_code(const std::string& code,
                      std::unique_ptr<clang::FrontendAction> action,
                      ParseSummary& out);
```

## 5. Drawer abstraction notes

- `Drawer::render` takes `TreeModel`, not clang AST. AST-specific knowledge lives
  in `AstBuilder` (fills Node fields) and `AstDrawer` (styles by category).
- The `NodeCategory` enum replaces the magic ints 0-3; JSON output is unaffected
  (category is only used for SVG styling).
- If a future model is a general graph rather than a tree, evolve `TreeModel` →
  `Diagram` with explicit edges; only the `Drawer::render` signature changes.

## 6. Implementation steps (for Code mode)

1. Create `model.h` — move `Node` verbatim; replace `int cat` with `NodeCategory`;
   add `TreeModel`.
2. Create `text_utils.h/.cpp` — move the six helpers unchanged.
3. Create `ast/ast_builder.h/.cpp` — move `AstBuilder` plus `AstCollector` and
   `AstDumpAction` (they reference `AstBuilder`); expose `tree()` returning
   `TreeModel`; keep walk/transparency/limit behavior identical.
4. Create `ast/clang_session.h/.cpp` — move `CollectingDiagConsumer` and
   `run_on_code` unchanged (rename to `run_tool_on_code` if desired).
5. Create `draw/drawer.h` — abstract `Drawer`. Create `draw/ast_drawer.h/.cpp` —
   move `render_svg` + `fill_for`/`stroke_for`/`text_for`, adapt signature to
   `TreeModel`.
6. Create `json_out.h/.cpp` — move `emit_node_json`, adapt to `TreeModel`.
7. Create `facade.h/.cpp` — `InputFacade` + `PassthroughStrategy`.
8. Rewrite `main.cpp` — read stdin, validate (blank/size), `facade.wrap`,
   `run_tool_on_code` with `AstDumpAction(builder)`, `AstDrawer` + `json_out`,
   compose the same JSON envelope (ok, node_count, truncated, errors, warnings,
   diagnostics, ast, svg).
9. Update `Makefile` — `SRC` becomes the new file list; keep the same
   `CXXFLAGS`/`LDFLAGS` machinery.
10. Build with `make`, run the sample from `web/index.html` through the new
    binary, and diff JSON/SVG output against the old binary — must be identical.

## 7. Acceptance criteria

- `make` builds `web/processor` with the split sources.
- Same JSON envelope shape; identical `ast` and `svg` for the sample snippet.
- `draw/drawer.h` and `model.h` contain no clang includes.
- Adding a new walker = new file under `ast/` + wiring in `main.cpp`.
- Adding a new drawer = new class implementing `Drawer`.

## 8. Clang AST walker guide (reference for future walkers)

### Step 0 — Ground truth
```
clang++ -Xclang -ast-dump -fsyntax-only -std=c++20 sample.cc
```
Judge every walker's output against this canonical dump.

### Step 1 — Pick a traversal style
| Style | What it is | Best for | Cost |
|-------|-----------|----------|------|
| `RecursiveASTVisitor` (CRTP) | override `Visit*` hooks; base class recurses | visit-every-X tools, counts, collections | low |
| ASTMatchers | declarative queries + `MatchFinder` callback | finding specific nodes (lint, rename) | medium setup |
| Manual traversal | own recursion via `decls()` + `Stmt::children()` | filtered, ordered trees for drawing | most code, most control |

RecursiveASTVisitor sketch:
```cpp
class FnCounter : public clang::RecursiveASTVisitor<FnCounter> {
public:
    bool VisitFunctionDecl(clang::FunctionDecl* f) { ++count; return true; }
    int count = 0;
    // optional: bool shouldVisitTemplateInstantiations() const { return true; }
};
```

### Step 2 — Wire into the tooling
```
ASTConsumer::HandleTranslationUnit(ASTContext&)   ← walker entry
        ▲
ASTFrontendAction::CreateASTConsumer()            ← returns the consumer
        ▲
ToolInvocation + FileManager                      ← drives the parse in-process
```
One-shot alternative: `clang::tooling::runToolOnCodeWithArgs`.

### Step 3 — Key APIs a walker touches
| Need | API |
|------|-----|
| descend decls | `DeclContext::decls()`, `FunctionDecl::parameters()`, `FunctionDecl::getBody()` |
| descend stmts | `Stmt::children()` (iterator range of `Stmt*`) |
| typed extraction | `isa<T>` / `dyn_cast<T>` (via `clang/Basic/LLVM.h`, no RTTI) |
| names | `NamedDecl::getDeclName().getAsString()` |
| types | `ValueDecl::getType().getAsString()`, `Expr::getType()`, `FunctionDecl::getReturnType()` |
| locations | `SourceManager::getPresumedLoc()`, `Decl/Stmt::getSourceRange()` |
| generic labels | `Decl::getDeclKindName()`, `Stmt::getStmtClassName()` |
| expr detection | `Expr` is-a `Stmt`; test `isa<Expr>(s)` first |

### Step 4 — Design the output model
- Indexed tree (children as indices) survives vector growth and serializes to JSON.
- Current-parent cursor saved/restored around recursion = enter/leave pattern
  (see `AstBuilder::walkDecl`/`walkStmt`).
- Cap nodes and depth to contain template/instantiation explosion.

### Step 5 — Filter deliberately
- Skip `d->isImplicit()` (compiler-generated decls).
- Decide whether wrapper exprs (`ImplicitCastExpr`, `MaterializeTemporaryExpr`,
  `ExprWithCleanups`, ...) are noise to hoist or signal to keep — per walker.
- Decide template instantiation policy up front.

### Step 6 — Validate
- Diff against `-ast-dump`; keep a small corpus of snippets exercising decls,
  stmts, exprs, templates, and implicit code.