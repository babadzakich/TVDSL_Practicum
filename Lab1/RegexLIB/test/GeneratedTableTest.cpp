// Проверка экспортированного заголовка funny_lexer_table.h (генерируется при сборке):
// он должен работать ровно так же, как библиотечный лексер, — это то, что пойдёт в T0.

#include <gtest/gtest.h>

#include <random>

#include "TestUtil.h"
#include "funny_lexer_table.h"

using funnylex::test::funny_build;
namespace gen = funny_lexer;

// Таблица constexpr — работает даже на этапе компиляции.
static_assert(gen::next_token("while(", 0).kind == gen::TokenKind::KW_WHILE);
static_assert(gen::next_token("while(", 0).length == 5);
static_assert(gen::next_token("01", 0).kind == gen::TokenKind::BAD_INT);
static_assert(gen::next_token("\xD0\xB6", 0).kind == gen::TokenKind::ERROR);
static_assert(gen::next_token("\xD0\xB6", 0).length == 2);
static_assert(gen::kTrapState == 0 && gen::kStartState == 1);

TEST(GeneratedTable, SameTablesAsLibrary) {
  const funnylex::LexerTable& t = funny_build().table;
  ASSERT_EQ(gen::kNumStates, t.num_states());
  ASSERT_EQ(gen::kNumClasses, t.num_classes);
  ASSERT_EQ(gen::kNumTokens, static_cast<int>(t.token_names.size()));
  for (int b = 0; b < 256; ++b) {
    EXPECT_EQ(gen::kByteClass[b], t.byte_class[b]);
  }
  for (int s = 0; s < t.num_states(); ++s) {
    EXPECT_EQ(gen::kAccept[s], t.accept[s]);
    for (int c = 0; c < t.num_classes; ++c) {
      EXPECT_EQ(gen::kNext[s][c], t.next[s][c]);
    }
  }
  for (int i = 0; i < gen::kNumTokens; ++i) {
    EXPECT_EQ(gen::kTokenNames[i], t.token_names[i]);
    EXPECT_EQ(gen::kSkip[i], t.skip[i]);
    EXPECT_EQ(gen::kIsError[i], t.is_error[i]);
  }
}

TEST(GeneratedTable, SameTokensAsLibraryLexer) {
  const funnylex::Lexer lexer(funny_build().table);
  const std::vector<std::string> pieces = {"if",   "iff",      "x_1",  "0",  "01", "42",  "==",     "=",
                                           "!",    "!=",       "<=",   "->", "=>", "//c", "\n",     " ",
                                           "\r\n", "\xD0\xB6", "\xFF", "@",  "(",  "}",   "length", ";"};
  std::mt19937 rng(7);
  std::uniform_int_distribution<size_t> pick(0, pieces.size() - 1);
  std::uniform_int_distribution<int> length(1, 12);
  for (int iter = 0; iter < 5000; ++iter) {
    std::string input;
    for (int k = length(rng); k > 0; --k) {
      input += pieces[pick(rng)];
    }
    size_t pos = 0;
    while (pos < input.size()) {
      const funnylex::Token expected = lexer.next(input, pos);
      const gen::Match actual = gen::next_token(input, pos);
      ASSERT_EQ(static_cast<int>(actual.kind), expected.kind) << '"' << input << "\" @" << pos;
      ASSERT_EQ(actual.length, expected.lexeme.size()) << '"' << input << "\" @" << pos;
      pos += actual.length;
    }
  }
}
