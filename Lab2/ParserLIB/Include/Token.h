// Слой токенов поверх табличного лексера HW1: позиции, пропуск пробелов и
// комментариев, лексические ошибки.

#ifndef FUNNYPARSE_TOKEN_H
#define FUNNYPARSE_TOKEN_H

#include <string>
#include <string_view>
#include <vector>

#include "Diagnostic.h"
#include "funny_lexer_table.h"

namespace funny {

using funny_lexer::TokenKind;

inline constexpr TokenKind kEof = static_cast<TokenKind>(funny_lexer::kNumTokens);

struct Token {
  TokenKind kind;
  std::string_view text;
  Pos pos;
};

struct LexResult {
  std::vector<Token> tokens;
  std::vector<Diagnostic> diagnostics;
};

LexResult lex(std::string_view source);

std::string describe_kind(TokenKind kind);

std::string quote_lexeme(std::string_view s);

}

#endif  // FUNNYPARSE_TOKEN_H
