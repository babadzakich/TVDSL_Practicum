// Таблица переходов лексера и табличный лексер (максимальное совпадение).

#ifndef FUNNYLEX_LEXER_H
#define FUNNYLEX_LEXER_H

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "Dfa.h"
#include "TokenSpec.h"

namespace funnylex {

struct LexerTable {
  int start = 1;
  int trap = 0;
  int num_classes = 0;
  std::array<int, 256> byte_class{};
  std::vector<std::vector<int>> next;
  std::vector<int> accept;
  std::vector<std::string> token_names;
  std::vector<bool> skip;
  std::vector<bool> is_error;

  int num_states() const { return static_cast<int>(next.size()); }
  int step(int state, unsigned char byte) const { return next[state][byte_class[byte]]; }
};

LexerTable make_lexer_table(const Dfa& min_dfa, const TokenSpec& spec);

struct BuildResult {
  TokenSpec spec;
  Nfa nfa;
  Dfa dfa;
  Dfa min_dfa;  
  LexerTable table;
};

BuildResult build_lexer(const TokenSpec& spec);

constexpr int kErrorToken = -1;

struct Token {
  int kind = kErrorToken;
  std::string lexeme;
  size_t offset = 0;
};

std::string token_name(const LexerTable& table, int kind);
bool is_error_token(const LexerTable& table, int kind);

class Lexer {
 public:
  explicit Lexer(const LexerTable& table) : table_(table) {}

  Token next(std::string_view input, size_t pos) const;

  std::vector<Token> tokenize(std::string_view input, bool keep_skipped = false) const;

 private:
  const LexerTable& table_;
};

struct WholeMatch {
  enum class Status { ACCEPT, REJECT, TRAP };
  Status status = Status::REJECT;
  int token = -1;
};

WholeMatch match_whole(const LexerTable& table, std::string_view input);

}  // namespace funnylex

#endif  // FUNNYLEX_LEXER_H
