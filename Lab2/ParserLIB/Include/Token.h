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

// Конец входа — вне диапазона токенов лексера.
inline constexpr TokenKind kEof = static_cast<TokenKind>(funny_lexer::kNumTokens);

struct Token {
  TokenKind kind;
  std::string_view text;  // указывает в исходный текст
  Pos pos;
};

struct LexResult {
  std::vector<Token> tokens;  // всегда заканчивается kEof
  std::vector<Diagnostic> diagnostics;
};

// WS и COMMENT пропускаются; BAD_INT сообщается и становится INT;
// соседние ERROR склеиваются в один токен и одно сообщение;
// не-ASCII в комментарии — одно сообщение, остаток строки пропускается.
LexResult lex(std::string_view source);

// "идентификатор", "число", "конец файла", "';'", "'while'".
std::string describe_kind(TokenKind kind);

// Лексема в одинарных кавычках; управляющие байты — \xHH.
std::string quote_lexeme(std::string_view s);

}  // namespace funny

#endif  // FUNNYPARSE_TOKEN_H
