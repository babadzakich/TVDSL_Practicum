#include "Ast.h"

#include "Overloaded.h"

namespace funny {

namespace {

std::vector<ExprPtr> clone_all(const std::vector<ExprPtr>& es) {
  std::vector<ExprPtr> out;
  out.reserve(es.size());
  for (const ExprPtr& e : es) {
    out.push_back(clone(*e));
  }
  return out;
}

}  // namespace

ExprPtr clone(const Expr& e) {
  auto node = std::visit(
      Overloaded{
          [](const IntLit& n) -> decltype(Expr::node) { return n; },
          [](const VarRef& v) -> decltype(Expr::node) { return v; },
          [](const ArrayAccess& a) -> decltype(Expr::node) { return ArrayAccess{a.array, clone(*a.index)}; },
          [](const Call& c) -> decltype(Expr::node) { return Call{c.callee, clone_all(c.args)}; },
          [](const Neg& n) -> decltype(Expr::node) { return Neg{clone(*n.operand)}; },
          [](const Binary& b) -> decltype(Expr::node) { return Binary{b.op, clone(*b.lhs), clone(*b.rhs)}; },
      },
      e.node);
  return std::make_unique<Expr>(Expr{e.pos, std::move(node), e.type});
}

std::string_view to_string(BinOp op) {
  switch (op) {
    case BinOp::Add: return "+";
    case BinOp::Sub: return "-";
    case BinOp::Mul: return "*";
    case BinOp::Div: return "/";
  }
  return "?";
}

std::string_view to_string(CmpOp op) {
  switch (op) {
    case CmpOp::Eq: return "==";
    case CmpOp::Ne: return "!=";
    case CmpOp::Ge: return ">=";
    case CmpOp::Le: return "<=";
    case CmpOp::Gt: return ">";
    case CmpOp::Lt: return "<";
  }
  return "?";
}

std::string to_string(TypeKind t) { return t == TypeKind::Int ? "int" : "int[]"; }

std::string to_string(const ExprType& t) {
  if (t.empty()) {
    return "?";
  }
  if (t.size() == 1) {
    return to_string(t[0]);
  }
  std::string out = "(";
  for (std::size_t i = 0; i < t.size(); ++i) {
    out += (i == 0 ? "" : ", ") + to_string(t[i]);
  }
  return out + ')';
}

std::string_view to_string(VarKind k) {
  switch (k) {
    case VarKind::Unresolved: return "";
    case VarKind::Param: return "param";
    case VarKind::Return: return "return";
    case VarKind::Local: return "local";
    case VarKind::Bound: return "bound";
  }
  return "?";
}

}  // namespace funny
