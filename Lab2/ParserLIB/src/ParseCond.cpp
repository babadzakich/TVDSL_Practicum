// Condition  <- COr ('->' Condition)?
// COr        <- CAnd ('or' CAnd)*
// CAnd       <- CNot ('and' CNot)*
// CNot       <- 'not' CNot / CAtom
// CAtom      <- 'true' / 'false' / Comparison / '(' Condition ')'
//
// Predicate  <- POr;  POr/PAnd/PNot — как у условий, без '->'
// PAtom      <- Quantifier / 'true' / 'false' / Comparison / FormulaRef / '(' Predicate ')'
// Quantifier <- ('forall' / 'exists') '(' VarDef '|' Predicate ')'
// FormulaRef <- IDENT '(' (Expr (',' Expr)*)? ')'
// Comparison <- Expr CmpOp Expr
//

#include <array>
#include <utility>

#include "ParserImpl.h"

namespace funny {

namespace {

CondPtr make_cond(Pos pos, decltype(Cond::node) node) { return std::make_unique<Cond>(Cond{pos, std::move(node)}); }

constexpr std::array<std::pair<TokenKind, CmpOp>, 6> kCmpOps = {{
    {TokenKind::EQ, CmpOp::Eq},
    {TokenKind::NE, CmpOp::Ne},
    {TokenKind::GE, CmpOp::Ge},
    {TokenKind::LE, CmpOp::Le},
    {TokenKind::GT, CmpOp::Gt},
    {TokenKind::LT, CmpOp::Lt},
}};

}  // namespace

// Левоассоциативная цепочка lhs (op rhs)*; op проверяется молча.
template <class Operand>
CondPtr ParserImpl::logical_chain(TokenKind op_token, LogicOp op, Operand operand) {
  CondPtr lhs = (this->*operand)();
  if (!lhs) {
    return nullptr;
  }
  ChainDepth chain(*this);
  while (at(op_token)) {
    const std::size_t m = mark();
    advance();
    CondPtr rhs = (this->*operand)();
    if (!rhs) {
      reset(m);
      break;
    }
    const Pos pos = lhs->pos;
    lhs = make_cond(pos, Logical{op, std::move(lhs), std::move(rhs)});
    chain.add();
  }
  return lhs;
}

// ---- условия ----

CondPtr ParserImpl::condition() {
  DepthGuard guard(*this);  // '->' рекурсивно вызывает condition()
  return labeled("условие", [&]() -> CondPtr {
    CondPtr lhs = logical_chain(TokenKind::KW_OR, LogicOp::Or, &ParserImpl::c_and);
    if (!lhs || !at(TokenKind::ARROW)) {
      return lhs;
    }
    const std::size_t m = mark();
    advance();
    CondPtr rhs = condition();
    if (!rhs) {
      reset(m);
      return lhs;
    }
    const Pos pos = lhs->pos;
    return make_cond(pos, Logical{LogicOp::Implies, std::move(lhs), std::move(rhs)});
  });
}

CondPtr ParserImpl::c_and() { return logical_chain(TokenKind::KW_AND, LogicOp::And, &ParserImpl::c_not); }

CondPtr ParserImpl::c_not() {
  DepthGuard guard(*this);
  return labeled("условие", [&]() -> CondPtr {
    const std::size_t start = mark();
    const Pos pos = peek().pos;
    if (accept(TokenKind::KW_NOT)) {
      CondPtr operand = c_not();
      if (!operand) {
        reset(start);
        return nullptr;
      }
      return make_cond(pos, Not{std::move(operand)});
    }
    return c_atom();
  });
}

CondPtr ParserImpl::c_atom() {
  if (CondPtr b = bool_lit()) {
    return b;
  }
  if (CondPtr c = comparison()) {
    return c;
  }
  return parenthesized(&ParserImpl::condition);
}

// ---- предикаты ----

CondPtr ParserImpl::predicate() {
  return labeled("предикат", [&] { return logical_chain(TokenKind::KW_OR, LogicOp::Or, &ParserImpl::p_and); });
}

CondPtr ParserImpl::p_and() { return logical_chain(TokenKind::KW_AND, LogicOp::And, &ParserImpl::p_not); }

CondPtr ParserImpl::p_not() {
  DepthGuard guard(*this);
  return labeled("предикат", [&]() -> CondPtr {
    const std::size_t start = mark();
    const Pos pos = peek().pos;
    if (accept(TokenKind::KW_NOT)) {
      CondPtr operand = p_not();
      if (!operand) {
        reset(start);
        return nullptr;
      }
      return make_cond(pos, Not{std::move(operand)});
    }
    return p_atom();
  });
}

CondPtr ParserImpl::p_atom() {
  if (CondPtr q = quantifier()) {
    return q;
  }
  if (CondPtr b = bool_lit()) {
    return b;
  }
  if (CondPtr c = comparison()) {
    return c;
  }
  if (CondPtr f = formula_ref()) {
    return f;
  }
  return parenthesized(&ParserImpl::predicate);
}

CondPtr ParserImpl::quantifier() {
  if (!at(TokenKind::KW_FORALL) && !at(TokenKind::KW_EXISTS)) {
    return nullptr;
  }
  const std::size_t start = mark();
  const Pos pos = peek().pos;
  const bool is_forall = at(TokenKind::KW_FORALL);
  advance();
  std::optional<VarDef> var;
  CondPtr body;
  if (!expect(TokenKind::LPAREN) || !(var = var_def()) || !expect(TokenKind::PIPE) || !(body = predicate()) ||
      !expect(TokenKind::RPAREN)) {
    reset(start);
    return nullptr;
  }
  return make_cond(pos, Quantifier{is_forall, std::move(*var), std::move(body)});
}

CondPtr ParserImpl::formula_ref() {
  if (!at(TokenKind::IDENT) || !at(TokenKind::LPAREN, 1)) {
    return nullptr;
  }
  const std::size_t start = mark();
  const Token& name = peek();
  advance();
  advance();
  std::optional<std::vector<ExprPtr>> args = call_args();
  if (!args) {
    reset(start);
    return nullptr;
  }
  return make_cond(name.pos, FormulaRef{std::string(name.text), std::move(*args)});
}

// ---- общее ----

CondPtr ParserImpl::bool_lit() {
  const Pos pos = peek().pos;
  if (accept(TokenKind::KW_TRUE)) {
    return make_cond(pos, BoolLit{true});
  }
  if (accept(TokenKind::KW_FALSE)) {
    return make_cond(pos, BoolLit{false});
  }
  return nullptr;
}

CondPtr ParserImpl::comparison() {
  const std::size_t start = mark();
  ExprPtr lhs = expr();
  if (!lhs) {
    return nullptr;
  }
  std::optional<CmpOp> op;
  for (const auto& [kind, cmp] : kCmpOps) {
    if (accept(kind)) {
      op = cmp;
      break;
    }
  }
  if (!op) {
    for (const auto& entry : kCmpOps) {
      fail(describe_kind(entry.first));
    }
    reset(start);
    return nullptr;
  }
  ExprPtr rhs = expr();
  if (!rhs) {
    reset(start);
    return nullptr;
  }
  const Pos pos = lhs->pos;
  return make_cond(pos, Compare{*op, std::move(lhs), std::move(rhs)});
}

CondPtr ParserImpl::parenthesized(CondPtr (ParserImpl::*inner)()) {
  const std::size_t start = mark();
  if (!accept(TokenKind::LPAREN)) {
    return nullptr;
  }
  CondPtr c = (this->*inner)();
  if (!c || !expect(TokenKind::RPAREN)) {
    reset(start);
    return nullptr;
  }
  return c;
}

// VarDef <- IDENT ':' Type
std::optional<VarDef> ParserImpl::var_def() {
  const std::size_t start = mark();
  const Token& name = peek();
  std::optional<TypeKind> t;
  if (!expect(TokenKind::IDENT) || !expect(TokenKind::COLON) || !(t = type())) {
    reset(start);
    return std::nullopt;
  }
  return VarDef{std::string(name.text), *t, name.pos};
}

// Type <- 'int' ('[' ']')?
std::optional<TypeKind> ParserImpl::type() {
  const std::size_t start = mark();
  if (!expect(TokenKind::KW_INT)) {
    return std::nullopt;
  }
  if (!expect(TokenKind::LBRACKET)) {
    return TypeKind::Int;
  }
  if (!expect(TokenKind::RBRACKET)) {
    reset(start);
    return std::nullopt;
  }
  return TypeKind::IntArray;
}

}  // namespace funny
