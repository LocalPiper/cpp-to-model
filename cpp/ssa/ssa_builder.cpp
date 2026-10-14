#include "ssa_builder.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>
#include <utility>

namespace cpp2model {

namespace {

std::vector<std::string> splitWs(const std::string &s) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      if (!cur.empty()) {
        out.push_back(std::move(cur));
        cur.clear();
      }
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty())
    out.push_back(std::move(cur));
  return out;
}

} // namespace

bool SsaBuilder::isTemp(const std::string &name) {
  return name.size() >= 6 && name.compare(0, 6, "__temp") == 0;
}

bool SsaBuilder::isNameToken(const std::string &tok) {
  if (tok.empty())
    return false;
  unsigned char c0 = static_cast<unsigned char>(tok[0]);
  if (!(std::isalpha(c0) || tok[0] == '_'))
    return false;
  for (char c : tok) {
    unsigned char u = static_cast<unsigned char>(c);
    if (!(std::isalnum(u) || c == '_'))
      return false;
  }
  return true;
}

bool SsaBuilder::isUnaryOp(const std::string &tok) {
  return tok == "!" || tok == "~" || tok == "-" || tok == "+" || tok == "*" ||
         tok == "&";
}

bool SsaBuilder::isSpecialUnary(const std::string &tok) {
  return tok == "sizeof" || tok == "alignof";
}

bool SsaBuilder::isBinaryOp(const std::string &tok) {
  static const char *const kOps[] = {
      "+", "-", "*", "/",  "%",  "<<", ">>", "&",  "|",
      "^", "<", ">", "<=", ">=", "==", "!=", "&&", "||",
  };
  for (const char *op : kOps)
    if (tok == op)
      return true;
  return false;
}

bool SsaBuilder::isSsaRenameable(const std::string &name) {
  return DefinedNames.count(name) > 0;
}

int SsaBuilder::intersect(int a, int b) {
  while (a != b) {
    while (RpoNum[a] > RpoNum[b])
      a = Idoms[a];
    while (RpoNum[b] > RpoNum[a])
      b = Idoms[b];
  }
  return a;
}

void SsaBuilder::computeRpo() {
  const int n = static_cast<int>(Ssa.blocks.size());
  Rpo.clear();
  Rpo.reserve(n);
  RpoNum.assign(n, -1);

  std::vector<bool> visited(n);

  std::vector<std::pair<int, size_t>> worklist;

  if (n > 0) {
    worklist.push_back({0, 0});
    visited[0] = true;
  }

  while (!worklist.empty()) {
    auto &[b, idx] = worklist.back();
    if (idx < Ssa.blocks[b].succs.size()) {
      int s = Ssa.blocks[b].succs[idx++];
      if (!visited[s]) {
        visited[s] = true;
        worklist.push_back({s, 0});
      }
    } else {
      Rpo.push_back(b);
      worklist.pop_back();
    }
  }

  std::reverse(Rpo.begin(), Rpo.end());
  for (int i = 0; i < static_cast<int>(Rpo.size()); ++i)
    RpoNum[Rpo[i]] = i;
}

void SsaBuilder::computeDominators() {
  const int n = static_cast<int>(Ssa.blocks.size());
  Idoms.assign(n, -1);

  if (n == 0)
    return;
  Idoms[0] = 0;

  bool changed = true;
  while (changed) {
    changed = false;
    for (int i = 1; i < static_cast<int>(Rpo.size()); ++i) {
      const int b = Rpo[i];
      int newIdom = -1;
      for (int p : Ssa.blocks[b].preds) {
        if (RpoNum[p] == -1)
          continue;
        if (Idoms[p] == -1)
          continue;
        newIdom = newIdom == -1 ? p : intersect(p, newIdom);
      }
      if (newIdom != -1 && newIdom != Idoms[b]) {
        Idoms[b] = newIdom;
        changed = true;
      }
    }
  }
}

void SsaBuilder::computeDominanceFrontiers() {
  const int n = static_cast<int>(Ssa.blocks.size());
  Df.assign(n, {});

  for (int b = 0; b < n; ++b) {
    if (RpoNum[b] == -1)
      continue;
    if (Idoms[b] == -1)
      continue;
    if (Ssa.blocks[b].preds.size() < 2)
      continue;

    const int stop = Idoms[b];
    for (int p : Ssa.blocks[b].preds) {
      if (RpoNum[p] == -1)
        continue;
      int runner = p;
      while (runner != stop) {
        if (runner < 0 || runner >= n)
          break;
        auto &vec = Df[runner];
        if (std::find(vec.begin(), vec.end(), b) == vec.end())
          vec.push_back(b);
        int next = Idoms[runner];
        if (next == -1)
          break;
        runner = next;
      }
    }
  }
}

void SsaBuilder::placePhis() {
  const int n = static_cast<int>(Ssa.blocks.size());

  std::unordered_map<std::string, std::vector<int>> defBlocks;

  for (int b = 0; b < n; ++b) {
    if (RpoNum[b] == -1)
      continue;
    for (const TacOp &op : Ssa.blocks[b].ops) {
      if (op.dst.empty())
        continue;
      if (isTemp(op.dst))
        continue;
      auto &v = defBlocks[op.dst];
      if (std::find(v.begin(), v.end(), b) == v.end())
        v.push_back(b);
    }
  }

  for (auto &[var, defs] : defBlocks) {
    std::vector<int> worklist = defs;

    auto blockHasPhi = [&](int b) -> bool {
      for (const PhiNode &p : Ssa.blocks[b].phis)
        if (p.base == var)
          return true;
      return false;
    };

    while (!worklist.empty()) {
      int b = worklist.back();
      worklist.pop_back();

      if (b < 0 || b >= n)
        continue;
      if (RpoNum[b] == -1)
        continue;
      if (b >= static_cast<int>(Df.size()))
        continue;

      for (int y : Df[b]) {
        if (y < 0 || y >= n)
          continue;
        if (RpoNum[y] == -1)
          continue;
        if (blockHasPhi(y))
          continue;

        Ssa.blocks[y].phis.push_back(PhiNode{var, var, {}});

        if (std::find(defs.begin(), defs.end(), y) == defs.end())
          worklist.push_back(y);
      }
    }
  }

  for (auto &bb : Ssa.blocks)
    std::sort(bb.phis.begin(), bb.phis.end(),
              [](const PhiNode &a, const PhiNode &b) { return a.var < b.var; });
}

void SsaBuilder::collectDefinedNames() {
  DefinedNames.clear();
  for (const auto &bb : Ssa.blocks) {
    for (const TacOp &op : bb.ops) {
      if (op.dst.empty())
        continue;
      if (!isNameToken(op.dst))
        continue;
      if (isTemp(op.dst))
        continue;
      DefinedNames.insert(op.dst);
    }
  }
}

void SsaBuilder::buildDomChildren() {
  const int n = static_cast<int>(Ssa.blocks.size());
  DomChildren.assign(n, {});
  for (int b = 1; b < n; ++b) {
    int d = Idoms[b];
    if (d >= 0 && d < n)
      DomChildren[d].push_back(b);
  }
}

void SsaBuilder::renameOp(TacOp &op) {
  for (auto &a : op.args)
    a = renameUse(a);
  if (!op.dst.empty())
    op.dst = renameDef(op.dst);
}

void SsaBuilder::renameFragment(std::string &field) {
  if (field.empty())
    return;
  ExprFragment f = parseExprFragment(field);
  for (auto &a : f.args)
    a = renameUse(a);
  if (f.args.size() == 1) {
    field = f.args[0];
  } else if (f.args.size() == 2) {
    field = f.args[0] + " " + f.op + " " + f.args[1];
  }
}

void SsaBuilder::renameTerminator(Terminator &t) {
  switch (t.Kind) {
  case TermKind::Branch:
    renameFragment(t.Cond);
    break;
  case TermKind::Switch:
    renameFragment(t.SwitchVal);
    break;
  case TermKind::Return:
    renameFragment(t.RetVal);
    break;
  default:
    break;
  }
}

void SsaBuilder::fillPhiInputs(int b) {
  for (int s : Ssa.blocks[b].succs) {
    if (s < 0 || s >= static_cast<int>(Ssa.blocks.size()))
      continue;
    for (PhiNode &phi : Ssa.blocks[s].phis) {
      auto it = VarStack.find(phi.base);
      std::string val;
      if (it == VarStack.end() || it->second.empty()) {
        val = phi.base + "0";
      } else {
        val = phi.base + std::to_string(it->second.back());
      }
      phi.incoming.push_back({val, b});
    }
  }
}

void SsaBuilder::renameBlock(int b) {
  std::vector<std::pair<std::string, size_t>> saved;

  for (PhiNode &phi : Ssa.blocks[b].phis) {
    saved.push_back({phi.base, VarStack[phi.base].size()});
    phi.var = renameDef(phi.base);
  }

  for (TacOp &op : Ssa.blocks[b].ops) {
    if (!op.dst.empty() && isSsaRenameable(op.dst))
      saved.push_back({op.dst, VarStack[op.dst].size()});
    renameOp(op);
  }

  renameTerminator(Ssa.blocks[b].term);

  fillPhiInputs(b);

  for (int c : DomChildren[b])
    renameBlock(c);

  for (auto it = saved.rbegin(); it != saved.rend(); ++it) {
    auto &stack = VarStack[it->first];
    while (stack.size() > it->second)
      stack.pop_back();
  }
}

void SsaBuilder::rename() {
  buildDomChildren();
  VarStack.clear();
  VarCounter.clear();

  if (!Ssa.blocks.empty())
    renameBlock(0);

  for (auto &bb : Ssa.blocks)
    for (auto &phi : bb.phis)
      std::sort(phi.incoming.begin(), phi.incoming.end(),
                [](const PhiInput &a, const PhiInput &b) {
                  return a.predBlock < b.predBlock;
                });
}

std::string SsaBuilder::renameUse(const std::string &name) {
  if (!isSsaRenameable(name))
    return name;
  auto it = VarStack.find(name);
  if (it == VarStack.end() || it->second.empty())
    return name + "0";
  return name + std::to_string(it->second.back());
}

std::string SsaBuilder::renameDef(const std::string &name) {
  if (!isSsaRenameable(name))
    return name;
  int &next = VarCounter[name];
  if (next == 0)
    next = 1;
  int v = next++;
  VarStack[name].push_back(v);
  return name + std::to_string(v);
}

TacOp SsaBuilder::parseTac(const std::string &stmt) {
  TacOp out;
  const auto eq = stmt.find(" = ");
  assert(eq != std::string::npos && "TAC statement must contain ' = '");

  out.dst = stmt.substr(0, eq);

  const std::string rhs = stmt.substr(eq + 3);
  const std::vector<std::string> toks = splitWs(rhs);
  assert(!toks.empty() && "TAC RHS must not be empty");

  const auto lp = rhs.find('(');
  if (lp != std::string::npos && rhs.back() == ')') {
    out.op = "call";
    out.args.push_back(rhs.substr(0, lp));
    const std::string inner = rhs.substr(lp + 1, rhs.size() - lp - 2);
    if (!inner.empty()) {
      size_t start = 0;
      while (true) {
        auto comma = inner.find(", ", start);
        if (comma == std::string::npos) {
          out.args.push_back(inner.substr(start));
          break;
        }
        out.args.push_back(inner.substr(start, comma - start));
        start = comma + 2;
      }
    }
    return out;
  }

  if (toks.size() == 1) {
    const std::string &t = toks[0];
    if (t.size() > 1 && (t[0] == '!' || t[0] == '~' || t[0] == '-' ||
                         t[0] == '+' || t[0] == '*' || t[0] == '&')) {
      out.op = std::string(1, t[0]);
      out.args.push_back(t.substr(1));
      return out;
    }
    out.op = "=";
    out.args.push_back(t);
    return out;
  }

  if (toks.size() == 2) {
    assert((isUnaryOp(toks[0]) || isSpecialUnary(toks[0])) &&
           "two-token TAC RHS must be a unary operator");
    out.op = toks[0];
    out.args.push_back(toks[1]);
    return out;
  }

  if (toks.size() == 3) {
    assert((isBinaryOp(toks[1]) || toks[1] == "=") &&
           "three-token TAC RHS must be binary");
    out.op = toks[1];
    out.args.push_back(toks[0]);
    out.args.push_back(toks[2]);
    return out;
  }

  assert(false && "unexpected TAC RHS arity");
  return out;
}

ExprFragment SsaBuilder::parseExprFragment(const std::string &expr) {
  ExprFragment out;
  const std::vector<std::string> toks = splitWs(expr);
  assert(!toks.empty() && "terminator expression must not be empty");

  if (toks.size() == 1) {
    out.args.push_back(toks[0]);
    return out;
  }
  if (toks.size() == 3) {
    assert(isBinaryOp(toks[1]) && "fragment middle token must be binary");
    out.op = toks[1];
    out.args.push_back(toks[0]);
    out.args.push_back(toks[2]);
    return out;
  }
  assert(false && "unexpected terminator expression arity");
  return out;
}

void SsaBuilder::computeEdges() {
  const int n = static_cast<int>(Ssa.blocks.size());
  for (int i = 0; i < n; ++i) {
    const Terminator &t = Ssa.blocks[i].term;
    auto addEdge = [&](int target) {
      assert(target >= 0 && target < n && "terminator points out of range");
      Ssa.blocks[i].succs.push_back(target);
      Ssa.blocks[target].preds.push_back(i);
    };
    switch (t.Kind) {
    case TermKind::Fallthrough:
      assert(false && "Fallthrough terminator survived into SSA");
      break;
    case TermKind::Jump:
      addEdge(t.Target);
      break;
    case TermKind::Branch:
      addEdge(t.OnTrue);
      addEdge(t.OnFalse);
      break;
    case TermKind::Switch:
      for (const auto &c : t.Cases)
        addEdge(c.second);
      addEdge(t.DefaultTarget);
      break;
    case TermKind::Return:
      break;
    }
  }
}

void SsaBuilder::parseBlocks(const Cfg &cfg) {
  Ssa.funcDecl = cfg.FuncDecl;
  Ssa.blocks.clear();
  Ssa.blocks.reserve(cfg.Blocks.size());

  for (const BB &bb : cfg.Blocks) {
    SsaBB out;
    out.term = bb.Term;
    out.ops.reserve(bb.Statements.size());
    for (const std::string &s : bb.Statements)
      out.ops.push_back(parseTac(s));
    Ssa.blocks.push_back(std::move(out));
  }
}

void SsaBuilder::build(const Cfg &cfg) {
  parseBlocks(cfg);
  computeEdges();
  computeRpo();
  computeDominators();
  computeDominanceFrontiers();
  placePhis();
  collectDefinedNames();
  rename();
  Ssa.rpoNum = RpoNum;
  Ssa.idom = Idoms;
  Ssa.df = Df;
}

} // namespace cpp2model