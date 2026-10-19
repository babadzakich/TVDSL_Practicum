// Внутренности PEG-парсера: курсор по токенам, учёт самой дальней ошибки,
// метки, мемоизация выражений, ограничение глубины.
//
// Контракт правил: при неудаче правило возвращает nullptr/nullopt и уже
// вернуло курсор туда, где начинало.

#ifndef FUNNYPARSE_PARSER_IMPL_H
#define FUNNYPARSE_PARSER_IMPL_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Ast.h"
#include "Parser.h"
#include "Token.h"

namespace funny {

struct DepthExceeded {
  Pos pos;
};

inline constexpr const char* kDepthMessage = "слишком глубокая вложенность (больше 1000 уровней)";

class ParserImpl {
 public:
  explicit ParserImpl(std::vector<Token> tokens);

  // ---- курсор ----
  const Token& peek(std::size_t ahead = 0) const;  // за концом — токен kEof
  bool at(TokenKind k, std::size_t ahead = 0) const;
  void advance();
  bool accept(TokenKind k);  // молча
  bool expect(TokenKind k);  // при неудаче записывает describe_kind(k)
  std::size_t mark() const { return pos_; }
  void reset(std::size_t m) { pos_ = m; }

  // ---- самая дальняя ошибка ----
  void fail(std::string what);  // в текущей позиции
  std::optional<Diagnostic> farthest_diagnostic() const;  // nullopt, если там ERROR-токен
  void clear_failures();                                  // и мемо тоже

  struct FailureState {
    std::size_t far = 0;
    std::vector<std::string> expected;
  };
  FailureState save_failures() const { return {far_, expected_}; }
  void restore_failures(FailureState s);

  // Если f не сработало, не продвинувшись дальше своего начала, — ожидаемым
  // в этой позиции становится label вместо подробностей.
  template <class F>
  auto labeled(std::string_view label, F&& f) -> decltype(f()) {
    const std::size_t start = pos_;
    FailureState saved = save_failures();
    auto r = f();
    if (!r && far_ <= start) {
      if (saved.far != start) {
        saved.expected.clear();
      }
      far_ = start;
      expected_ = std::move(saved.expected);
      add_expected(std::string(label));
    }
    return r;
  }

  class DepthGuard {
   public:
    explicit DepthGuard(ParserImpl& p);
    ~DepthGuard() { --p_.depth_; }
    DepthGuard(const DepthGuard&) = delete;
    DepthGuard& operator=(const DepthGuard&) = delete;

   private:
    ParserImpl& p_;
  };

  // Глубина для левых цепочек (a + b + ... , x and y and ...): каждый оператор
  // углубляет дерево на уровень, поэтому тоже считается в kMaxDepth.
  class ChainDepth {
   public:
    explicit ChainDepth(ParserImpl& p) : p_(p) {}
    ~ChainDepth() { p_.depth_ -= links_; }
    ChainDepth(const ChainDepth&) = delete;
    ChainDepth& operator=(const ChainDepth&) = delete;
    void add();

   private:
    ParserImpl& p_;
    int links_ = 0;
  };

  // ---- грамматика ----
  ExprPtr expr();
  CondPtr condition();
  CondPtr predicate();
  std::optional<VarDef> var_def();
  std::optional<TypeKind> type();
  StmtPtr statement();
  std::optional<Decl> decl();
  Module module(std::vector<Diagnostic>& out);

 private:
  bool formula_ahead();
  std::optional<Decl> formula();
  std::optional<Decl> function();
  std::optional<std::vector<VarDef>> paren_params();
  std::optional<LocalDef> local_def();
  StmtPtr block();
  void skip_statement();
  bool decl_header_ahead() const;
  void report_unclosed_brace(const Pos& open);
  void skip_to_next_decl();
  void report_farthest();  // самая дальняя ошибка — в out_, сброс учёта
  StmtPtr if_stmt();
  StmtPtr while_stmt();
  StmtPtr check_stmt();
  StmtPtr assignment();

  void add_expected(std::string what);

  template <class Operand>
  CondPtr logical_chain(TokenKind op_token, LogicOp op, Operand operand);
  CondPtr c_and();
  CondPtr c_not();
  CondPtr c_atom();
  CondPtr p_and();
  CondPtr p_not();
  CondPtr p_atom();
  CondPtr quantifier();
  CondPtr formula_ref();
  CondPtr bool_lit();
  CondPtr comparison();
  CondPtr parenthesized(CondPtr (ParserImpl::*inner)());

  ExprPtr expr_uncached();
  ExprPtr term();
  ExprPtr unary();
  ExprPtr primary();
  ExprPtr call();
  std::optional<std::vector<ExprPtr>> call_args();

  std::vector<Token> tokens_;
  std::size_t pos_ = 0;
  std::size_t far_ = 0;
  std::vector<std::string> expected_;
  int depth_ = 0;
  std::vector<Diagnostic>* out_ = nullptr;  // задаётся в module()
  std::optional<std::size_t> brace_reported_at_;  // где уже сообщили о незакрытой '{'

  struct MemoEntry {
    std::size_t end;
    ExprPtr result;  // nullptr — правило не сработало
  };
  std::unordered_map<std::size_t, MemoEntry> expr_memo_;
};

}  // namespace funny

#endif  // FUNNYPARSE_PARSER_IMPL_H
