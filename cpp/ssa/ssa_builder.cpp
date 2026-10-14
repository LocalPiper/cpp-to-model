#include "ssa_builder.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>
#include <utility>

namespace cpp2model {

namespace {

std::vector<std::string> split_ws(const std::string &s) {
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

bool SsaBuilder::is_temp(const std::string &name) {
  return name.size() >= 6 && name.compare(0, 6, "__temp") == 0;
}

bool SsaBuilder::is_name_token(const std::string &tok) {
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

bool SsaBuilder::is_unary_op(const std::string &tok) {
  return tok == "!" || tok == "~" || tok == "-" || tok == "+" || tok == "*" ||
         tok == "&";
}

bool SsaBuilder::is_special_unary(const std::string &tok) {
  return tok == "sizeof" || tok == "alignof";
}

bool SsaBuilder::is_binary_op(const std::string &tok) {
  static const char *const kOps[] = {
      "+", "-", "*", "/",  "%",  "<<", ">>", "&",  "|",
      "^", "<", ">", "<=", ">=", "==", "!=", "&&", "||",
  };
  for (const char *op : kOps)
    if (tok == op)
      return true;
  return false;
}

bool SsaBuilder::is_ssa_renameable(const std::string &name) {
  return m_defined_names.count(name) > 0;
}

int SsaBuilder::intersect(int a, int b) {
  while (a != b) {
    while (m_rpo_num[a] > m_rpo_num[b])
      a = m_idom[a];
    while (m_rpo_num[b] > m_rpo_num[a])
      b = m_idom[b];
  }
  return a;
}

void SsaBuilder::computeRpo() {
  const int n = static_cast<int>(m_ssa.blocks.size());
  m_rpo.clear();
  m_rpo.reserve(n);
  m_rpo_num.assign(n, -1);

  std::vector<bool> visited(n);

  std::vector<std::pair<int, size_t>> worklist;

  if (n > 0) {
    worklist.push_back({0, 0});
    visited[0] = true;
  }

  while (!worklist.empty()) {
    auto &[b, idx] = worklist.back();
    if (idx < m_ssa.blocks[b].succs.size()) {
      int s = m_ssa.blocks[b].succs[idx++];
      if (!visited[s]) {
        visited[s] = true;
        worklist.push_back({s, 0});
      }
    } else {
      m_rpo.push_back(b);
      worklist.pop_back();
    }
  }

  std::reverse(m_rpo.begin(), m_rpo.end());
  for (int i = 0; i < static_cast<int>(m_rpo.size()); ++i) {
    m_rpo_num[m_rpo[i]] = i;
  }
}

void SsaBuilder::computeDominators() {
  const int n = static_cast<int>(m_ssa.blocks.size());
  m_idom.assign(n, -1);

  if (n == 0)
    return;
  m_idom[0] = 0;

  bool changed = true;
  while (changed) {
    changed = false;
    for (int i = 1; i < static_cast<int>(m_rpo.size()); ++i) {
      const int b = m_rpo[i];
      int new_idom = -1;
      for (int p : m_ssa.blocks[b].preds) {
        if (m_rpo_num[p] == -1)
          continue; // unreachable block
        if (m_idom[p] == -1)
          continue; // pred's idom not set
        new_idom = new_idom == -1 ? p : intersect(p, new_idom);
      }
      if (new_idom != -1 && new_idom != m_idom[b]) {
        m_idom[b] = new_idom;
        changed = true;
      }
    }
  }
}

void SsaBuilder::computeDominanceFrontiers() {
  const int n = static_cast<int>(m_ssa.blocks.size());
  m_df.assign(n, {});

  for (int b = 0; b < n; ++b) {
    if (m_rpo_num[b] == -1)
      continue;
    if (m_idom[b] == -1)
      continue;
    if (m_ssa.blocks[b].preds.size() < 2)
      continue;

    const int stop = m_idom[b];
    for (int p : m_ssa.blocks[b].preds) {
      if (m_rpo_num[p] == -1)
        continue;
      int runner = p;
      while (runner != stop) {
        if (runner < 0 || runner >= n)
          break;
        auto &vec = m_df[runner];
        if (std::find(vec.begin(), vec.end(), b) == vec.end()) {
          vec.push_back(b);
        }
        int next = m_idom[runner];
        if (next == -1)
          break;
        runner = next;
      }
    }
  }
}

void SsaBuilder::placePhis() {
  const int n = static_cast<int>(m_ssa.blocks.size());

  std::unordered_map<std::string, std::vector<int>> def_blocks;

  for (int b = 0; b < n; ++b) {
    if (m_rpo_num[b] == -1)
      continue;
    for (const TacOp &op : m_ssa.blocks[b].ops) {
      if (op.dst.empty())
        continue;
      if (is_temp(op.dst))
        continue;
      auto &v = def_blocks[op.dst];
      if (std::find(v.begin(), v.end(), b) == v.end()) {
        v.push_back(b);
      }
    }
  }

  for (auto &[var, defs] : def_blocks) {
    std::vector<int> worklist = defs;

    auto block_has_phi = [&](int b) -> bool {
      for (const PhiNode &p : m_ssa.blocks[b].phis) {
        if (p.base == var)
          return true;
      }
      return false;
    };

    while (!worklist.empty()) {
      int b = worklist.back();
      worklist.pop_back();

      if (b < 0 || b >= n)
        continue;
      if (m_rpo_num[b] == -1)
        continue;
      if (b >= static_cast<int>(m_df.size()))
        continue;

      for (int y : m_df[b]) {
        if (y < 0 || y >= n)
          continue;
        if (m_rpo_num[y] == -1)
          continue;
        if (block_has_phi(y))
          continue;

        m_ssa.blocks[y].phis.push_back(PhiNode{var, var, {}});

        if (std::find(defs.begin(), defs.end(), y) == defs.end()) {
          worklist.push_back(y);
        }
      }
    }
  }

  for (auto &bb : m_ssa.blocks) {
    std::sort(bb.phis.begin(), bb.phis.end(),
              [](const PhiNode &a, const PhiNode &b) { return a.var < b.var; });
  }
}

void SsaBuilder::collectDefinedNames() {
  m_defined_names.clear();
  for (const auto &bb : m_ssa.blocks) {
    for (const TacOp &op : bb.ops) {
      if (op.dst.empty())
        continue;
      if (!is_name_token(op.dst))
        continue;
      if (is_temp(op.dst))
        continue;
      m_defined_names.insert(op.dst);
    }
  }
}

void SsaBuilder::buildDomChildren() {
  const int n = static_cast<int>(m_ssa.blocks.size());
  m_dom_children.assign(n, {});
  for (int b = 1; b < n; ++b) {
    int d = m_idom[b];
    if (d >= 0 && d < n) {
      m_dom_children[d].push_back(b);
    }
  }
}

void SsaBuilder::renameOp(TacOp &op) {
  for (auto &a : op.args)
    a = rename_use(a);
  if (!op.dst.empty())
    op.dst = rename_def(op.dst);
}

void SsaBuilder::renameFragment(std::string &field) {
  if (field.empty())
    return;
  ExprFragment f = parse_expr_fragment(field);
  for (auto &a : f.args)
    a = rename_use(a);
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
  for (int s : m_ssa.blocks[b].succs) {
    if (s < 0 || s >= static_cast<int>(m_ssa.blocks.size()))
      continue;
    for (PhiNode &phi : m_ssa.blocks[s].phis) {
      auto it = m_var_stack.find(phi.base);
      std::string val;
      if (it == m_var_stack.end() || it->second.empty()) {
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

  for (PhiNode &phi : m_ssa.blocks[b].phis) {
    saved.push_back({phi.base, m_var_stack[phi.base].size()});
    phi.var = rename_def(phi.base);
  }

  for (TacOp &op : m_ssa.blocks[b].ops) {
    if (!op.dst.empty() && is_ssa_renameable(op.dst)) {
      saved.push_back({op.dst, m_var_stack[op.dst].size()});
    }
    renameOp(op);
  }

  renameTerminator(m_ssa.blocks[b].term);

  fillPhiInputs(b);

  for (int c : m_dom_children[b]) {
    renameBlock(c);
  }

  for (auto it = saved.rbegin(); it != saved.rend(); ++it) {
    auto &stack = m_var_stack[it->first];
    while (stack.size() > it->second)
      stack.pop_back();
  }
}

void SsaBuilder::rename() {
  buildDomChildren();
  m_var_stack.clear();
  m_var_counter.clear();

  if (!m_ssa.blocks.empty()) {
    renameBlock(0);
  }

  for (auto &bb : m_ssa.blocks) {
    for (auto &phi : bb.phis) {
      std::sort(phi.incoming.begin(), phi.incoming.end(),
                [](const PhiInput &a, const PhiInput &b) {
                  return a.pred_block < b.pred_block;
                });
    }
  }
}

std::string SsaBuilder::rename_use(const std::string &name) {
  if (!is_ssa_renameable(name))
    return name;
  auto it = m_var_stack.find(name);
  if (it == m_var_stack.end() || it->second.empty()) {
    return name + "0";
  }
  return name + std::to_string(it->second.back());
}

std::string SsaBuilder::rename_def(const std::string &name) {
  if (!is_ssa_renameable(name))
    return name;
  int &next = m_var_counter[name];
  if (next == 0)
    next = 1;
  int v = next++;
  m_var_stack[name].push_back(v);
  return name + std::to_string(v);
}

TacOp SsaBuilder::parse_tac(const std::string &stmt) {
  TacOp out;
  const auto eq = stmt.find(" = ");
  assert(eq != std::string::npos && "TAC statement must contain ' = '");

  out.dst = stmt.substr(0, eq);

  const std::string rhs = stmt.substr(eq + 3);
  const std::vector<std::string> toks = split_ws(rhs);
  assert(!toks.empty() && "TAC RHS must not be empty");

  if (toks.size() == 1) {
    const std::string &t = toks[0];
    // glued unary prefix
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
    assert((is_unary_op(toks[0]) || is_special_unary(toks[0])) &&
           "two-token TAC RHS must be a unary operator");
    out.op = toks[0];
    out.args.push_back(toks[1]);
    return out;
  }

  if (toks.size() == 3) {
    assert((is_binary_op(toks[1]) || toks[1] == "=") &&
           "three-token TAC RHS must be binary");
    out.op = toks[1];
    out.args.push_back(toks[0]);
    out.args.push_back(toks[2]);
    return out;
  }

  assert(false && "unexpected TAC RHS arity");
  return out;
}

ExprFragment SsaBuilder::parse_expr_fragment(const std::string &expr) {
  ExprFragment out;
  const std::vector<std::string> toks = split_ws(expr);
  assert(!toks.empty() && "terminator expression must not be empty");

  if (toks.size() == 1) {
    out.args.push_back(toks[0]);
    return out;
  }
  if (toks.size() == 3) {
    assert(is_binary_op(toks[1]) && "fragment middle token must be binary");
    out.op = toks[1];
    out.args.push_back(toks[0]);
    out.args.push_back(toks[2]);
    return out;
  }
  assert(false && "unexpected terminator expression arity");
  return out;
}

void SsaBuilder::computeEdges() {
  const int n = static_cast<int>(m_ssa.blocks.size());
  for (int i = 0; i < n; ++i) {
    const Terminator &t = m_ssa.blocks[i].term;
    auto add_edge = [&](int target) {
      assert(target >= 0 && target < n && "terminator points out of range");
      m_ssa.blocks[i].succs.push_back(target);
      m_ssa.blocks[target].preds.push_back(i);
    };
    switch (t.Kind) {
    case TermKind::Fallthrough:
      assert(false && "Fallthrough terminator survived into SSA");
      break;
    case TermKind::Jump:
      add_edge(t.Target);
      break;
    case TermKind::Branch:
      add_edge(t.OnTrue);
      add_edge(t.OnFalse);
      break;
    case TermKind::Switch:
      for (const auto &c : t.Cases)
        add_edge(c.second);
      add_edge(t.DefaultTarget);
      break;
    case TermKind::Return:
      break;
    }
  }
}

void SsaBuilder::parseBlocks(const Cfg &cfg) {
  m_ssa.func_decl = cfg.FuncDecl;
  m_ssa.blocks.clear();
  m_ssa.blocks.reserve(cfg.Blocks.size());

  for (const BB &bb : cfg.Blocks) {
    SsaBB out;
    out.term = bb.Term;
    out.ops.reserve(bb.Statements.size());
    for (const std::string &s : bb.Statements) {
      out.ops.push_back(parse_tac(s));
    }
    m_ssa.blocks.push_back(std::move(out));
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
  m_ssa.rpo_num = m_rpo_num;
  m_ssa.idom = m_idom;
  m_ssa.df = m_df;
}

} // namespace cpp2model