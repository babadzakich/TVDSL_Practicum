#include "AstPrinter.h"

#include "Overloaded.h"

namespace funny {

namespace {

class TreePrinter {
 public:
  std::string take() { return std::move(out_); }

  void expr(const Expr& e, int depth) {
    std::visit(Overloaded{
                   [&](const IntLit& n) { line(depth, "IntLit " + n.text, e.pos); },
                   [&](const VarRef& v) { line(depth, "Var " + v.name, e.pos); },
                   [&](const ArrayAccess& a) {
                     line(depth, "ArrayAccess " + a.array, e.pos);
                     expr(*a.index, depth + 1);
                   },
                   [&](const Call& c) {
                     line(depth, "Call " + c.callee, e.pos);
                     exprs(c.args, depth + 1);
                   },
                   [&](const Neg& n) {
                     line(depth, "Neg", e.pos);
                     expr(*n.operand, depth + 1);
                   },
                   [&](const Binary& b) {
                     line(depth, "Binary " + std::string(to_string(b.op)), e.pos);
                     expr(*b.lhs, depth + 1);
                     expr(*b.rhs, depth + 1);
                   },
               },
               e.node);
  }

  void cond(const Cond& c, int depth) {
    std::visit(Overloaded{
                   [&](const BoolLit& b) { line(depth, b.value ? "BoolLit true" : "BoolLit false", c.pos); },
                   [&](const Compare& cmp) {
                     line(depth, "Compare " + std::string(to_string(cmp.op)), c.pos);
                     expr(*cmp.lhs, depth + 1);
                     expr(*cmp.rhs, depth + 1);
                   },
                   [&](const Not& n) {
                     line(depth, "Not", c.pos);
                     cond(*n.operand, depth + 1);
                   },
                   [&](const Logical& l) {
                     line(depth, logic_name(l.op), c.pos);
                     cond(*l.lhs, depth + 1);
                     cond(*l.rhs, depth + 1);
                   },
                   [&](const Quantifier& q) {
                     line(depth,
                          std::string(q.is_forall ? "Quantifier forall " : "Quantifier exists ") + q.var.name + ": " +
                              to_string(q.var.type),
                          c.pos);
                     cond(*q.body, depth + 1);
                   },
                   [&](const FormulaRef& f) {
                     line(depth, "FormulaRef " + f.name, c.pos);
                     exprs(f.args, depth + 1);
                   },
               },
               c.node);
  }

  void stmt(const Stmt& s, int depth) {
    std::visit(Overloaded{
                   [&](const Block& b) {
                     line(depth, "Block", s.pos);
                     for (const StmtPtr& child : b.body) {
                       stmt(*child, depth + 1);
                     }
                   },
                   [&](const If& i) {
                     line(depth, "If", s.pos);
                     group(depth + 1, "cond");
                     cond(*i.cond, depth + 2);
                     group(depth + 1, "then");
                     stmt(*i.then_branch, depth + 2);
                     if (i.else_branch) {
                       group(depth + 1, "else");
                       stmt(*i.else_branch, depth + 2);
                     }
                   },
                   [&](const While& w) {
                     line(depth, "While", s.pos);
                     group(depth + 1, "cond");
                     cond(*w.cond, depth + 2);
                     if (w.invariant) {
                       group(depth + 1, "invariant");
                       cond(*w.invariant, depth + 2);
                     }
                     group(depth + 1, "body");
                     stmt(*w.body, depth + 2);
                   },
                   [&](const Assert& a) {
                     line(depth, "Assert", s.pos);
                     cond(*a.pred, depth + 1);
                   },
                   [&](const Assume& a) {
                     line(depth, "Assume", s.pos);
                     cond(*a.pred, depth + 1);
                   },
                   [&](const Assign& a) {
                     line(depth,
                          "Assign " + a.target.text + (a.sugar == Sugar::ArrayUpdate ? " [sugar: array_update]" : ""),
                          s.pos);
                     expr(*a.value, depth + 1);
                   },
                   [&](const TupleAssign& t) {
                     std::string names;
                     for (const Name& n : t.targets) {
                       names += (names.empty() ? "" : ", ") + n.text;
                     }
                     line(depth, "TupleAssign " + names, s.pos);
                     expr(*t.call, depth + 1);
                   },
               },
               s.node);
  }

  void decl(const Decl& d, int depth) {
    std::visit(Overloaded{
                   [&](const Function& f) {
                     line(depth, "Function " + f.name.text, f.name.pos);
                     var_defs(depth + 1, "params", f.params);
                     if (f.requires_clause) {
                       group(depth + 1, "requires");
                       cond(*f.requires_clause, depth + 2);
                     }
                     var_defs(depth + 1, "returns", f.returns);
                     if (f.ensures_clause) {
                       group(depth + 1, "ensures");
                       cond(*f.ensures_clause, depth + 2);
                     }
                     if (!f.uses.empty()) {
                       group(depth + 1, "uses");
                       for (const LocalDef& l : f.uses) {
                         line(depth + 2, l.type ? l.name + ": " + to_string(*l.type) : l.name, l.pos);
                       }
                     }
                     group(depth + 1, "body");
                     stmt(*f.body, depth + 2);
                   },
                   [&](const Formula& f) {
                     line(depth, "Formula " + f.name.text, f.name.pos);
                     var_defs(depth + 1, "params", f.params);
                     group(depth + 1, "body");
                     cond(*f.body, depth + 2);
                   },
               },
               d);
  }

  void module(const Module& m) {
    out_ += "Module\n";
    for (const Decl& d : m.decls) {
      decl(d, 1);
    }
  }

 private:
  void group(int depth, const char* name) {
    out_.append(static_cast<std::size_t>(depth) * 2, ' ');
    out_ += name;
    out_ += '\n';
  }

  void var_defs(int depth, const char* name, const std::vector<VarDef>& vs) {
    if (vs.empty()) {
      return;
    }
    group(depth, name);
    for (const VarDef& v : vs) {
      line(depth + 1, v.name + ": " + to_string(v.type), v.pos);
    }
  }

  static std::string logic_name(LogicOp op) {
    switch (op) {
      case LogicOp::And: return "And";
      case LogicOp::Or: return "Or";
      case LogicOp::Implies: return "Implies";
    }
    return "?";
  }

  void line(int depth, const std::string& text, const Pos& pos) {
    out_.append(static_cast<std::size_t>(depth) * 2, ' ');
    out_ += text + " @" + std::to_string(pos.line) + ':' + std::to_string(pos.col) + '\n';
  }

  void exprs(const std::vector<ExprPtr>& es, int depth) {
    for (const ExprPtr& e : es) {
      expr(*e, depth);
    }
  }

  std::string out_;
};

}  // namespace

std::string print_tree(const Expr& e) {
  TreePrinter p;
  p.expr(e, 0);
  return p.take();
}

std::string print_tree(const Cond& c) {
  TreePrinter p;
  p.cond(c, 0);
  return p.take();
}

std::string print_tree(const Stmt& s) {
  TreePrinter p;
  p.stmt(s, 0);
  return p.take();
}

std::string print_tree(const Module& m) {
  TreePrinter p;
  p.module(m);
  return p.take();
}

}  // namespace funny
