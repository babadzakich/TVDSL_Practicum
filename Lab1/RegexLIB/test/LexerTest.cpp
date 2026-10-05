#include <gtest/gtest.h>

#include <map>
#include <random>

#include "TestUtil.h"
#include "Export.h"
#include "TestCases.h"

using namespace funnylex;
using funnylex::test::funny_build;

TEST(Lexer, CasesFile) {
  const LexerTable& table = funny_build().table;
  const std::vector<TestCase> cases = load_test_cases("tests/cases.txt");
  ASSERT_GT(cases.size(), 100u);
  for (const TestCase& test : cases) {
    const TestOutcome outcome = run_test_case(table, test);
    EXPECT_TRUE(outcome.passed) << "line " << test.line << ": " << format_test_case(test)
                                << "\n  actual: " << outcome.actual;
  }
}

// Каждый из 256 байтов по отдельности: либо конкретный токен, либо ловушка.
TEST(Lexer, EverySingleByteIsCovered) {
  const LexerTable& table = funny_build().table;
  const TokenSpec& spec = funny_build().spec;
  std::map<char, std::string> expected = {
      {' ', "WS"},       {'\t', "WS"},      {'\r', "WS"},    {'\n', "WS"},    {'(', "LPAREN"}, {')', "RPAREN"},
      {'[', "LBRACKET"}, {']', "RBRACKET"}, {'{', "LBRACE"}, {'}', "RBRACE"}, {',', "COMMA"},  {';', "SEMICOLON"},
      {':', "COLON"},    {'|', "PIPE"},     {'+', "PLUS"},   {'-', "MINUS"},  {'*', "STAR"},   {'/', "SLASH"},
      {'<', "LT"},       {'>', "GT"},       {'=', "ASSIGN"}};
  for (char c = '0'; c <= '9'; ++c) {
    expected[c] = "INT";
  }
  for (char c = 'a'; c <= 'z'; ++c) {
    expected[c] = "IDENT";
    expected[static_cast<char>(c - 'a' + 'A')] = "IDENT";
  }

  const Lexer lexer(table);
  for (int byte = 0; byte < 256; ++byte) {
    const std::string input(1, static_cast<char>(byte));
    const std::vector<Token> tokens = lexer.tokenize(input, /*keep_skipped=*/true);
    ASSERT_EQ(tokens.size(), 1u) << "byte " << byte;
    const auto it = expected.find(static_cast<char>(byte));
    const std::string want = it != expected.end() ? it->second : "ERROR";
    EXPECT_EQ(token_name(table, tokens[0].kind), want) << "byte " << byte;

    const WholeMatch m = match_whole(table, input);
    if (want == "ERROR") {
      // '!' — префикс "!=", поэтому REJECT; остальные сразу в ловушку.
      EXPECT_EQ(m.status, byte == '!' ? WholeMatch::Status::REJECT : WholeMatch::Status::TRAP) << "byte " << byte;
    } else {
      EXPECT_EQ(m.status, WholeMatch::Status::ACCEPT);
      EXPECT_EQ(m.token, spec.find(want));
    }
    if (byte >= 128) {
      EXPECT_EQ(table.step(table.start, static_cast<unsigned char>(byte)), table.trap);
    }
  }
}

TEST(Lexer, SkipTokensAreKeptOnRequest) {
  const Lexer lexer(funny_build().table);
  const auto tokens = lexer.tokenize("x // c\n\ty", true);
  ASSERT_EQ(tokens.size(), 5u);
  EXPECT_EQ(tokens[1].lexeme, " ");
  EXPECT_EQ(tokens[2].lexeme, "// c");
  EXPECT_EQ(tokens[3].lexeme, "\n\t");
  EXPECT_EQ(tokens[4].offset, 8u);
}

TEST(Lexer, AlwaysProgressesAndCoversInput) {
  const Lexer lexer(funny_build().table);
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> byte(0, 255);
  std::uniform_int_distribution<int> length(0, 40);
  for (int iter = 0; iter < 5000; ++iter) {
    std::string input;
    for (int k = length(rng); k > 0; --k) {
      input += static_cast<char>(byte(rng));
    }
    std::string joined;
    size_t expected_offset = 0;
    for (const Token& t : lexer.tokenize(input, true)) {
      ASSERT_FALSE(t.lexeme.empty());
      ASSERT_EQ(t.offset, expected_offset);
      expected_offset += t.lexeme.size();
      joined += t.lexeme;
    }
    ASSERT_EQ(joined, input);
  }
}

TEST(Lexer, CaseFileParser) {
  const auto cases = parse_test_cases(
      "# comment\n"
      "match | \"a\\tb\" | TRAP\n"
      "lex | \"\" |\n"
      "lex | \"x\\x41\" | IDENT=\"xA\" # trailing comment\n");
  ASSERT_EQ(cases.size(), 3u);
  EXPECT_EQ(cases[0].input, "a\tb");
  EXPECT_TRUE(cases[1].expected.empty());
  EXPECT_EQ(cases[2].expected, std::vector<std::string>{"IDENT=\"xA\""});
  EXPECT_THROW(parse_test_cases("oops | \"a\" | X"), std::runtime_error);
  EXPECT_THROW(parse_test_cases("lex | \"a | X"), std::runtime_error);
  EXPECT_THROW(parse_test_cases("match | \"a\" | X Y"), std::runtime_error);
}

TEST(Export, JsonIsProduced) {
  const BuildResult& b = funny_build();
  const std::string json = export_json(b);
  EXPECT_NE(json.find("\"trap\": 0"), std::string::npos);
  EXPECT_NE(json.find("\"start\": 1"), std::string::npos);
  EXPECT_NE(json.find("\"name\": \"KW_WHILE\""), std::string::npos);
  EXPECT_EQ(escape_string("a\"\\\n\xFF"), "a\\\"\\\\\\n\\xFF");
}
