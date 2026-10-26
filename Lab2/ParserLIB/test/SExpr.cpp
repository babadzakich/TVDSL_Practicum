#include "SExpr.h"

#include <variant>

using namespace funny;

namespace {

template <class... Fs>
struct Overloaded : Fs... {
  using Fs::operator()...;
};

std::string list(std::string head, const std::vector<ExprPtr>& items) {
  for (const ExprPtr& e : items) {
    head += ' ' + sexpr(*e);
  }
  return '(' + head + ')';
}

std::string var_defs(const char* head, const std::vector<VarDef>& vs) {
  if (vs.empty()) {
    return "";
  }
  std::string out = std::string(" (") + head;
  for (const VarDef& v : vs) {
    out += ' ' + v.name + ':' + to_string(v.type);
  }
  return out + ')';
}

}  // namespace

std::string sexpr(const Expr& e) {
  return std::visit(
      Overloaded{
          [](const IntLit& n) { return n.text; },
          [](const VarRef& v) { return v.name; },
          [](const ArrayAccess& a) { return "(index " + a.array + ' ' + sexpr(*a.index) + ')'; },
          [](const Call& c) { return list("call " + c.callee, c.args); },
          [](const Neg& n) { return "(neg " + sexpr(*n.operand) + ')'; },
          [](const Binary& b) {
            return '(' + std::string(to_string(b.op)) + ' ' + sexpr(*b.lhs) + ' ' + sexpr(*b.rhs) + ')';
          },
      },
      e.node);
}

std::string sexpr(const Cond& c) {
  return std::visit(
      Overloaded{
          [](const BoolLit& b) -> std::string { return b.value ? "true" : "false"; },
          [](const Compare& cmp) {
            return '(' + std::string(to_string(cmp.op)) + ' ' + sexpr(*cmp.lhs) + ' ' + sexpr(*cmp.rhs) + ')';
          },
          [](const Not& n) { return "(not " + sexpr(*n.operand) + ')'; },
          [](const Logical& l) {
            const char* op = l.op == LogicOp::And ? "and" : l.op == LogicOp::Or ? "or" : "->";
            return std::string("(") + op + ' ' + sexpr(*l.lhs) + ' ' + sexpr(*l.rhs) + ')';
          },
          [](const Quantifier& q) {
            return std::string(q.is_forall ? "(forall " : "(exists ") + q.var.name + ':' + to_string(q.var.type) +
                   ' ' + sexpr(*q.body) + ')';
          },
          [](const FormulaRef& f) { return list("ref " + f.name, f.args); },
      },
      c.node);
}

std::string sexpr(const Stmt& s) {
  return std::visit(
      Overloaded{
          [](const Block& b) {
            std::string out = "(block";
            for (const StmtPtr& child : b.body) {
              out += ' ' + sexpr(*child);
            }
            return out + ')';
          },
          [](const If& i) {
            return "(if " + sexpr(*i.cond) + ' ' + sexpr(*i.then_branch) +
                   (i.else_branch ? ' ' + sexpr(*i.else_branch) : "") + ')';
          },
          [](const While& w) {
            return "(while " + sexpr(*w.cond) + (w.invariant ? " (inv " + sexpr(*w.invariant) + ')' : "") + ' ' +
                   sexpr(*w.body) + ')';
          },
          [](const Assert& a) { return "(assert " + sexpr(*a.pred) + ')'; },
          [](const Assume& a) { return "(assume " + sexpr(*a.pred) + ')'; },
          [](const Assign& a) {
            return std::string(a.sugar == Sugar::ArrayUpdate ? "(=sugar " : "(= ") + a.target.text + ' ' +
                   sexpr(*a.value) + ')';
          },
          [](const TupleAssign& t) {
            std::string names;
            for (const Name& n : t.targets) {
              names += (names.empty() ? "" : " ") + n.text;
            }
            return "(= (" + names + ") " + sexpr(*t.call) + ')';
          },
      },
      s.node);
}

std::string sexpr(const Decl& d) {
  return std::visit(
      Overloaded{
          [](const Function& f) {
            std::string out = "(function " + f.name.text + var_defs("params", f.params);
            if (f.requires_clause) {
              out += " (requires " + sexpr(*f.requires_clause) + ')';
            }
            out += var_defs("returns", f.returns);
            if (f.ensures_clause) {
              out += " (ensures " + sexpr(*f.ensures_clause) + ')';
            }
            if (!f.uses.empty()) {
              out += " (uses";
              for (const LocalDef& l : f.uses) {
                out += ' ' + l.name + (l.type ? ':' + to_string(*l.type) : "");
              }
              out += ')';
            }
            return out + ' ' + sexpr(*f.body) + ')';
          },
          [](const Formula& f) {
            return "(formula " + f.name.text + var_defs("params", f.params) + ' ' + sexpr(*f.body) + ')';
          },
      },
      d);
}
