// Табличные тесты лексера (формат — tests/cases.txt).
//
//   match | "строка" | ОЖИДАНИЕ      — вся строка прогоняется через ДКА;
//                                     ОЖИДАНИЕ = ИМЯ_ТОКЕНА | REJECT | TRAP
//   lex   | "строка" | T1 T2="x" ...  — разбиение на токены (skip-токены не выдаются);
//                                     T="лексема" дополнительно сверяет лексему,
//                                     ERROR — символ, попавший в ловушку.

#ifndef FUNNYLEX_TEST_CASES_H
#define FUNNYLEX_TEST_CASES_H

#include <string>
#include <vector>

#include "Lexer.h"

namespace funnylex {

struct TestCase {
  enum class Kind { MATCH, LEX };
  Kind kind = Kind::LEX;
  std::string input;
  std::vector<std::string> expected;
  int line = 0;
};

std::vector<TestCase> parse_test_cases(const std::string& text);
std::vector<TestCase> load_test_cases(const std::string& path);

struct TestOutcome {
  bool passed = false;
  std::string actual;
};

TestOutcome run_test_case(const LexerTable& table, const TestCase& test);

std::string format_test_case(const TestCase& test);

}  // namespace funnylex

#endif  // FUNNYLEX_TEST_CASES_H
