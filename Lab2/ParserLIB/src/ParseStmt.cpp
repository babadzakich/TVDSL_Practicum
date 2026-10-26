// Module       <- Decl+ EOF
// Decl         <- Formula / Function            (выбор по &(IDENT '(' Params? ')' '=>'))
// Formula      <- IDENT '(' Params? ')' '=>' Predicate
// Function     <- IDENT '(' Params? ')' ('requires' Predicate)? 'returns' VarDef (',' VarDef)*
//                 ('ensures' Predicate)? ('uses' LocalDef (',' LocalDef)*)? Statement
// Statement    <- Block / If / While / Assert / Assume / Assignment   (выбор по первому токену)
// Assignment   <- TupleAssign / ArrayAssign / SimpleAssign            (выбор по второму токену)

#include "ParserImpl.h"

namespace funny {

namespace {

StmtPtr make_stmt(Pos pos, decltype(Stmt::node) node) { return std::make_unique<Stmt>(Stmt{pos, std::move(node)}); }

ExprPtr make_expr(Pos pos, decltype(Expr::node) node) { return std::make_unique<Expr>(Expr{pos, std::move(node)}); }

}  // namespace

// ---- модуль и объявления ----

Module ParserImpl::module(std::vector<Diagnostic>& out) {
  out_ = &out;
  Module m;
  do {
    const std::size_t start = mark();
    if (std::optional<Decl> d = decl()) {
      m.decls.push_back(std::move(*d));
      continue;
    }
    report_farthest();
    reset(start);
    advance();
    skip_to_next_decl();
  } while (!at(kEof));
  return m;
}

void ParserImpl::report_farthest() {
  if (std::optional<Diagnostic> d = farthest_diagnostic()) {
    out_->push_back(std::move(*d));
  }
  clear_failures();
}

// Восстановление на уровне модуля: до IDENT '(' на нулевой глубине скобок,
// перед которым '}' или ';' либо который начинает строку.
void ParserImpl::skip_to_next_decl() {
  int braces = 0;
  int parens = 0;
  while (!at(kEof)) {
    const Token& t = peek();
    if (braces == 0 && parens == 0 && t.kind == TokenKind::IDENT && at(TokenKind::LPAREN, 1)) {
      const Token& prev = tokens_[pos_ - 1];
      if (prev.kind == TokenKind::RBRACE || prev.kind == TokenKind::SEMICOLON || prev.pos.line != t.pos.line) {
        return;
      }
    }
    switch (t.kind) {  // незакрытая '(' в заголовке не должна тянуться дальше '{', '}', ';'
      case TokenKind::LBRACE:
        ++braces;
        parens = 0;
        break;
      case TokenKind::RBRACE:
        braces = braces > 0 ? braces - 1 : 0;
        parens = 0;
        break;
      case TokenKind::SEMICOLON: parens = 0; break;
      case TokenKind::LPAREN: ++parens; break;
      case TokenKind::RPAREN: parens = parens > 0 ? parens - 1 : 0; break;
      default: break;
    }
    advance();
  }
}

std::optional<Decl> ParserImpl::decl() {
  return labeled("объявление функции или формулы", [&]() -> std::optional<Decl> {
    if (formula_ahead()) {
      return formula();
    }
    return function();
  });
}

// &(IDENT '(' Params? ')' '=>') — без записи ожидаемого.
bool ParserImpl::formula_ahead() {
  const std::size_t start = mark();
  const FailureState saved = save_failures();
  const bool ok = accept(TokenKind::IDENT) && paren_params() && accept(TokenKind::FAT_ARROW);
  reset(start);
  restore_failures(saved);
  return ok;
}

std::optional<Decl> ParserImpl::formula() {
  const std::size_t start = mark();
  const Token& name = peek();
  advance();
  std::optional<std::vector<VarDef>> params = paren_params();
  CondPtr body;
  if (!params || !expect(TokenKind::FAT_ARROW) || !(body = predicate())) {
    reset(start);
    return std::nullopt;
  }
  return Formula{Name{std::string(name.text), name.pos}, std::move(*params), std::move(body)};
}

std::optional<Decl> ParserImpl::function() {
  const std::size_t start = mark();
  const Token& name = peek();
  auto fail_here = [&]() -> std::optional<Decl> {
    reset(start);
    return std::nullopt;
  };
  if (!expect(TokenKind::IDENT)) {
    return fail_here();
  }
  Function f;
  f.name = Name{std::string(name.text), name.pos};
  std::optional<std::vector<VarDef>> params = paren_params();
  if (!params) {
    return fail_here();
  }
  f.params = std::move(*params);
  if (expect(TokenKind::KW_REQUIRES) && !(f.requires_clause = predicate())) {
    return fail_here();
  }
  if (!expect(TokenKind::KW_RETURNS)) {
    return fail_here();
  }
  do {
    std::optional<VarDef> r = var_def();
    if (!r) {
      return fail_here();
    }
    f.returns.push_back(std::move(*r));
  } while (expect(TokenKind::COMMA));
  if (expect(TokenKind::KW_ENSURES) && !(f.ensures_clause = predicate())) {
    return fail_here();
  }
  if (expect(TokenKind::KW_USES)) {
    do {
      std::optional<LocalDef> l = local_def();
      if (!l) {
        return fail_here();
      }
      f.uses.push_back(std::move(*l));
    } while (expect(TokenKind::COMMA));
  }
  if (!(f.body = statement())) {
    return fail_here();
  }
  return f;
}

// '(' (VarDef (',' VarDef)*)? ')'
std::optional<std::vector<VarDef>> ParserImpl::paren_params() {
  const std::size_t start = mark();
  if (!expect(TokenKind::LPAREN)) {
    return std::nullopt;
  }
  std::vector<VarDef> params;
  if (std::optional<VarDef> first = var_def()) {
    params.push_back(std::move(*first));
    while (true) {
      const std::size_t m = mark();
      if (!expect(TokenKind::COMMA)) {
        break;
      }
      std::optional<VarDef> next = var_def();
      if (!next) {
        reset(m);
        break;
      }
      params.push_back(std::move(*next));
    }
  }
  if (!expect(TokenKind::RPAREN)) {
    reset(start);
    return std::nullopt;
  }
  return params;
}

// LocalDef <- IDENT (':' Type)?
std::optional<LocalDef> ParserImpl::local_def() {
  const std::size_t start = mark();
  const Token& name = peek();
  if (!expect(TokenKind::IDENT)) {
    return std::nullopt;
  }
  LocalDef l{std::string(name.text), std::nullopt, name.pos};
  if (expect(TokenKind::COLON) && !(l.type = type())) {
    reset(start);
    return std::nullopt;
  }
  return l;
}

// ---- операторы ----

StmtPtr ParserImpl::statement() {
  DepthGuard guard(*this);
  return labeled("оператор", [&]() -> StmtPtr {
    switch (peek().kind) {
      case TokenKind::LBRACE: return block();
      case TokenKind::KW_IF: return if_stmt();
      case TokenKind::KW_WHILE: return while_stmt();
      case TokenKind::KW_ASSERT:
      case TokenKind::KW_ASSUME: return check_stmt();
      case TokenKind::IDENT: return assignment();
      default: return nullptr;
    }
  });
}

// Block <- '{' Statement* '}'
// Ошибочный оператор сообщается и пропускается (skip_statement), блок при этом
// считается разобранным. Незакрытая '{' сообщается у конца файла или перед
// заголовком следующего объявления — и блок на этом заканчивается.
StmtPtr ParserImpl::block() {
  const Pos pos = peek().pos;
  if (!expect(TokenKind::LBRACE)) {
    return nullptr;
  }
  Block b;
  while (!accept(TokenKind::RBRACE)) {
    if (at(kEof) || decl_header_ahead()) {
      report_unclosed_brace(pos);
      break;
    }
    const std::size_t start = mark();
    if (StmtPtr s = statement()) {
      b.body.push_back(std::move(s));
      continue;
    }
    report_farthest();
    reset(start);
    skip_statement();
  }
  return make_stmt(pos, std::move(b));
}

// Пропуск ошибочного оператора: до ';' или до '}', закрывающей вложенный блок
// (включительно), — а если дальше 'else', то и его ветки. '}' текущего уровня не
// поглощается. Оператор не начинается ни с '}', ни с конца файла, так что хотя
// бы один токен поглощается.
void ParserImpl::skip_statement() {
  int depth = 0;
  while (!at(kEof)) {
    const TokenKind k = peek().kind;
    if (k == TokenKind::RBRACE && depth == 0) {
      return;
    }
    advance();
    bool ended = false;
    if (k == TokenKind::LBRACE) {
      ++depth;
    } else if (k == TokenKind::RBRACE) {
      ended = --depth == 0;
    } else if (k == TokenKind::SEMICOLON) {
      ended = depth == 0;
    }
    if (ended && !accept(TokenKind::KW_ELSE)) {
      return;
    }
  }
}

// &(IDENT '(' ... ')' ('requires' / 'returns' / '=>')) — начало следующего объявления.
bool ParserImpl::decl_header_ahead() const {
  if (!at(TokenKind::IDENT) || !at(TokenKind::LPAREN, 1)) {
    return false;
  }
  int parens = 1;
  std::size_t i = pos_ + 2;
  for (; i < tokens_.size() && parens > 0; ++i) {
    switch (tokens_[i].kind) {
      case TokenKind::LPAREN: ++parens; break;
      case TokenKind::RPAREN: --parens; break;
      case TokenKind::LBRACE:
      case TokenKind::RBRACE:
      case TokenKind::SEMICOLON: return false;
      default: break;
    }
  }
  if (parens > 0 || i >= tokens_.size()) {
    return false;
  }
  const TokenKind next = tokens_[i].kind;
  return next == TokenKind::KW_REQUIRES || next == TokenKind::KW_RETURNS || next == TokenKind::FAT_ARROW;
}

// Одно сообщение на место: вложенные незакрытые блоки не дублируют его.
void ParserImpl::report_unclosed_brace(const Pos& open) {
  if (brace_reported_at_ == pos_) {
    return;
  }
  brace_reported_at_ = pos_;
  std::string found = at(kEof) ? "найден конец файла" : "найдено " + quote_lexeme(peek().text);
  out_->push_back({peek().pos, "ожидается '}' (для '{' в " + std::to_string(open.line) + ':' +
                                   std::to_string(open.col) + "), " + found});
}

// If <- 'if' '(' Condition ')' Statement ('else' Statement)?
StmtPtr ParserImpl::if_stmt() {
  const std::size_t start = mark();
  const Pos pos = peek().pos;
  advance();
  If s;
  if (!expect(TokenKind::LPAREN) || !(s.cond = condition()) || !expect(TokenKind::RPAREN) ||
      !(s.then_branch = statement())) {
    reset(start);
    return nullptr;
  }
  if (expect(TokenKind::KW_ELSE) && !(s.else_branch = statement())) {
    reset(start);
    return nullptr;
  }
  return make_stmt(pos, std::move(s));
}

// While <- 'while' '(' Condition ')' ('invariant' Predicate)? Statement
StmtPtr ParserImpl::while_stmt() {
  const std::size_t start = mark();
  const Pos pos = peek().pos;
  advance();
  While s;
  if (!expect(TokenKind::LPAREN) || !(s.cond = condition()) || !expect(TokenKind::RPAREN) ||
      (expect(TokenKind::KW_INVARIANT) && !(s.invariant = predicate())) || !(s.body = statement())) {
    reset(start);
    return nullptr;
  }
  return make_stmt(pos, std::move(s));
}

// Assert <- 'assert' Predicate ';'    Assume <- 'assume' Predicate ';'
StmtPtr ParserImpl::check_stmt() {
  const std::size_t start = mark();
  const Pos pos = peek().pos;
  const bool is_assert = at(TokenKind::KW_ASSERT);
  advance();
  CondPtr pred = predicate();
  if (!pred || !expect(TokenKind::SEMICOLON)) {
    reset(start);
    return nullptr;
  }
  if (is_assert) {
    return make_stmt(pos, Assert{std::move(pred)});
  }
  return make_stmt(pos, Assume{std::move(pred)});
}

// TupleAssign  <- IDENT (',' IDENT)+ '=' Call ';'
// ArrayAssign  <- IDENT '[' Expr ']' '=' Expr ';'    (a[i] = v  ==>  a = set(a, i, v))
// SimpleAssign <- IDENT '=' Expr ';'
StmtPtr ParserImpl::assignment() {
  const std::size_t start = mark();
  const Token& first = peek();
  const Name target{std::string(first.text), first.pos};
  advance();
  auto fail_here = [&]() -> StmtPtr {
    reset(start);
    return nullptr;
  };

  if (at(TokenKind::COMMA)) {
    TupleAssign t;
    t.targets.push_back(target);
    while (expect(TokenKind::COMMA)) {
      const Token& name = peek();
      if (!expect(TokenKind::IDENT)) {
        return fail_here();
      }
      t.targets.push_back(Name{std::string(name.text), name.pos});
    }
    if (!expect(TokenKind::ASSIGN) || !(t.call = labeled("вызов функции", [&] { return call(); })) ||
        !expect(TokenKind::SEMICOLON)) {
      return fail_here();
    }
    return make_stmt(first.pos, std::move(t));
  }

  if (accept(TokenKind::LBRACKET)) {
    ExprPtr index;
    ExprPtr value;
    if (!(index = expr()) || !expect(TokenKind::RBRACKET) || !expect(TokenKind::ASSIGN) || !(value = expr()) ||
        !expect(TokenKind::SEMICOLON)) {
      return fail_here();
    }
    std::vector<ExprPtr> args;
    args.push_back(make_expr(first.pos, VarRef{target.text}));
    args.push_back(std::move(index));
    args.push_back(std::move(value));
    ExprPtr set = make_expr(first.pos, Call{"set", std::move(args)});
    return make_stmt(first.pos, Assign{target, std::move(set), Sugar::ArrayUpdate});
  }

  ExprPtr value;
  if (!expect(TokenKind::ASSIGN) || !(value = expr()) || !expect(TokenKind::SEMICOLON)) {
    return fail_here();
  }
  return make_stmt(first.pos, Assign{target, std::move(value), Sugar::None});
}

}  // namespace funny
