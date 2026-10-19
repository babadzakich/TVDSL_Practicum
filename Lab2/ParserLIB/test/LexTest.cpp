#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "Diagnostic.h"
#include "Token.h"

using namespace funny;

namespace {

// Имена токенов через пробел; kEof печатается как EOF.
std::string kinds(const LexResult& r) {
  std::string out;
  for (const Token& t : r.tokens) {
    if (!out.empty()) {
      out += ' ';
    }
    out += t.kind == kEof ? "EOF" : funny_lexer::token_name(t.kind);
  }
  return out;
}

}  // namespace

TEST(Lex, SkipsWhitespaceAndComments) {
  auto r = lex("x // c\n  y");
  EXPECT_EQ(kinds(r), "IDENT IDENT EOF");
  EXPECT_EQ(r.tokens[1].pos.line, 2);
  EXPECT_EQ(r.tokens[1].pos.col, 3);
  EXPECT_EQ(r.tokens[2].pos.col, 4);
  EXPECT_TRUE(r.diagnostics.empty());
}

TEST(Lex, EmptyInputIsJustEof) {
  auto r = lex("");
  EXPECT_EQ(kinds(r), "EOF");
  EXPECT_EQ(r.tokens[0].pos.col, 1);
}

TEST(Lex, IntArrayIsThreeTokens) { EXPECT_EQ(kinds(lex("int[]")), "KW_INT LBRACKET RBRACKET EOF"); }

TEST(Lex, CrLfLines) {
  auto r = lex("a\r\nb");
  EXPECT_EQ(r.tokens[1].pos.line, 2);
  EXPECT_EQ(r.tokens[1].pos.col, 1);
}

TEST(Lex, SingleBadCharStaysInStream) {
  auto r = lex("x = @;");
  EXPECT_EQ(kinds(r), "IDENT ASSIGN ERROR SEMICOLON EOF");
  ASSERT_EQ(r.diagnostics.size(), 1u);
  EXPECT_EQ(format_diagnostic("t", r.diagnostics[0]), "t:1:5: ошибка: недопустимый символ '@'");
}

TEST(Lex, AdjacentBadBytesCoalesceAndColumnsCountBytes) {
  auto r = lex("жж x");  // 4 байта вне ASCII
  EXPECT_EQ(kinds(r), "ERROR IDENT EOF");
  EXPECT_EQ(r.tokens[1].pos.col, 6);
  ASSERT_EQ(r.diagnostics.size(), 1u);
  EXPECT_EQ(format_diagnostic("t", r.diagnostics[0]),
            "t:1:1: ошибка: недопустимые символы '\\xD0\\xB6\\xD0\\xB6'");
}

TEST(Lex, ControlBytesAreEscaped) {
  auto r = lex(std::string_view("\0\x01", 2));
  ASSERT_EQ(r.diagnostics.size(), 1u);
  EXPECT_EQ(r.diagnostics[0].message, "недопустимые символы '\\x00\\x01'");
}

TEST(Lex, BadIntBecomesIntWithOneDiagnostic) {
  auto r = lex("x = 007;");
  EXPECT_EQ(kinds(r), "IDENT ASSIGN INT SEMICOLON EOF");
  EXPECT_EQ(r.tokens[2].text, "007");
  ASSERT_EQ(r.diagnostics.size(), 1u);
  EXPECT_EQ(format_diagnostic("t", r.diagnostics[0]),
            "t:1:5: ошибка: некорректное число '007': ведущие нули не допускаются");
}

TEST(Lex, NonAsciiCommentReportedOnceRestOfLineSkipped) {
  auto r = lex("x = 1; // привет world\ny");
  EXPECT_EQ(kinds(r), "IDENT ASSIGN INT SEMICOLON IDENT EOF");
  ASSERT_EQ(r.diagnostics.size(), 1u);
  EXPECT_EQ(format_diagnostic("t", r.diagnostics[0]),
            "t:1:11: ошибка: недопустимые символы в комментарии (разрешён только ASCII)");
}

TEST(Lex, DescribeKind) {
  EXPECT_EQ(describe_kind(TokenKind::IDENT), "идентификатор");
  EXPECT_EQ(describe_kind(TokenKind::INT), "число");
  EXPECT_EQ(describe_kind(kEof), "конец файла");
  EXPECT_EQ(describe_kind(TokenKind::SEMICOLON), "';'");
  EXPECT_EQ(describe_kind(TokenKind::KW_RETURNS), "'returns'");
  EXPECT_EQ(describe_kind(TokenKind::FAT_ARROW), "'=>'");
}

TEST(Lex, SortDiagnosticsByOffsetStable) {
  std::vector<Diagnostic> ds = {{Pos{1, 5, 4}, "b"}, {Pos{1, 1, 0}, "a"}, {Pos{1, 5, 4}, "c"}};
  sort_diagnostics(ds);
  EXPECT_EQ(ds[0].message, "a");
  EXPECT_EQ(ds[1].message, "b");
  EXPECT_EQ(ds[2].message, "c");
}

TEST(Lex, NonAsciiBytesAreEscaped) {
  auto r = lex(std::string_view("\xFF\xFE x", 4));
  ASSERT_EQ(r.diagnostics.size(), 1u);
  EXPECT_EQ(r.diagnostics[0].message, "недопустимые символы '\\xFF\\xFE'");
  EXPECT_EQ(quote_lexeme("a\x7F\x80"), "'a\\x7F\\x80'");
}
