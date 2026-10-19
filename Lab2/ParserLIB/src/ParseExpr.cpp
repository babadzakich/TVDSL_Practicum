// Expr    <- Term (('+' / '-') Term)*
// Term    <- Unary (('*' / '/') Unary)*
// Unary   <- '-' Unary / Primary
// Primary <- Call / ArrayAccess / INT / IDENT / '(' Expr ')'
//
// Продолжения циклов операторов проверяются молча. Call/ArrayAccess/Var
// выбираются по токену после IDENT — это то же упорядоченное выбирание,
// альтернативы расходятся на втором токене.

#include "ParserImpl.h"

namespace funny {

namespace {

ExprPtr make_expr(Pos pos, decltype(Expr::node) node) { return std::make_unique<Expr>(Expr{pos, std::move(node)}); }

}  // namespace

ExprPtr ParserImpl::expr() {
  const std::size_t start = pos_;
  if (auto it = expr_memo_.find(start); it != expr_memo_.end()) {
    pos_ = it->second.end;
    return it->second.result ? clone(*it->second.result) : nullptr;
  }
  ExprPtr r = expr_uncached();
  expr_memo_[start] = MemoEntry{pos_, r ? clone(*r) : nullptr};
  return r;
}

ExprPtr ParserImpl::expr_uncached() {
  ExprPtr lhs = term();
  if (!lhs) {
    return nullptr;
  }
  ChainDepth chain(*this);
  while (at(TokenKind::PLUS) || at(TokenKind::MINUS)) {
    const std::size_t m = mark();
    const BinOp op = at(TokenKind::PLUS) ? BinOp::Add : BinOp::Sub;
    advance();
    ExprPtr rhs = term();
    if (!rhs) {
      reset(m);
      break;
    }
    const Pos pos = lhs->pos;
    lhs = make_expr(pos, Binary{op, std::move(lhs), std::move(rhs)});
    chain.add();
  }
  return lhs;
}

ExprPtr ParserImpl::term() {
  ExprPtr lhs = unary();
  if (!lhs) {
    return nullptr;
  }
  ChainDepth chain(*this);
  while (at(TokenKind::STAR) || at(TokenKind::SLASH)) {
    const std::size_t m = mark();
    const BinOp op = at(TokenKind::STAR) ? BinOp::Mul : BinOp::Div;
    advance();
    ExprPtr rhs = unary();
    if (!rhs) {
      reset(m);
      break;
    }
    const Pos pos = lhs->pos;
    lhs = make_expr(pos, Binary{op, std::move(lhs), std::move(rhs)});
    chain.add();
  }
  return lhs;
}

ExprPtr ParserImpl::unary() {
  DepthGuard guard(*this);
  return labeled("выражение", [&]() -> ExprPtr {
    const std::size_t start = mark();
    const Pos pos = peek().pos;
    if (accept(TokenKind::MINUS)) {
      ExprPtr operand = unary();
      if (!operand) {
        reset(start);
        return nullptr;
      }
      return make_expr(pos, Neg{std::move(operand)});
    }
    return primary();
  });
}

ExprPtr ParserImpl::primary() {
  const Token& t = peek();
  const std::size_t start = mark();
  switch (t.kind) {
    case TokenKind::INT:
      advance();
      return make_expr(t.pos, IntLit{std::string(t.text)});
    case TokenKind::KW_LENGTH:
      return call();
    case TokenKind::IDENT:
      if (at(TokenKind::LPAREN, 1)) {
        return call();
      }
      if (at(TokenKind::LBRACKET, 1)) {
        advance();
        advance();
        ExprPtr index = expr();
        if (!index || !expect(TokenKind::RBRACKET)) {
          reset(start);
          return nullptr;
        }
        return make_expr(t.pos, ArrayAccess{std::string(t.text), std::move(index)});
      }
      advance();
      return make_expr(t.pos, VarRef{std::string(t.text)});
    case TokenKind::LPAREN: {
      advance();
      ExprPtr inner = expr();
      if (!inner || !expect(TokenKind::RPAREN)) {
        reset(start);
        return nullptr;
      }
      return inner;
    }
    default:
      return nullptr;
  }
}

// Call <- (IDENT / 'length') '(' (Expr (',' Expr)*)? ')'
ExprPtr ParserImpl::call() {
  const Token& name = peek();
  const std::size_t start = mark();
  if (!at(TokenKind::IDENT) && !at(TokenKind::KW_LENGTH)) {
    fail(describe_kind(TokenKind::IDENT));
    return nullptr;
  }
  advance();
  std::optional<std::vector<ExprPtr>> args;
  if (!expect(TokenKind::LPAREN) || !(args = call_args())) {
    reset(start);
    return nullptr;
  }
  return make_expr(name.pos, Call{std::string(name.text), std::move(*args)});
}

// Аргументы после '(' включая закрывающую ')'.
std::optional<std::vector<ExprPtr>> ParserImpl::call_args() {
  const std::size_t start = mark();
  std::vector<ExprPtr> args;
  if (ExprPtr first = expr()) {
    args.push_back(std::move(first));
    while (true) {
      const std::size_t m = mark();
      if (!expect(TokenKind::COMMA)) {
        break;
      }
      ExprPtr next = expr();
      if (!next) {
        reset(m);
        break;
      }
      args.push_back(std::move(next));
    }
  }
  if (!expect(TokenKind::RPAREN)) {
    reset(start);
    return std::nullopt;
  }
  return args;
}

}  // namespace funny
