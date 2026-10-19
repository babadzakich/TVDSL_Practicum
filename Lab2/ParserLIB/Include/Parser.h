// PEG-парсер Funny поверх токенов лексера HW1.

#ifndef FUNNYPARSE_PARSER_H
#define FUNNYPARSE_PARSER_H

#include <string_view>
#include <vector>

#include "Ast.h"
#include "Diagnostic.h"

namespace funny {

inline constexpr int kMaxDepth = 1000;

struct ParseResult {
  Module module;                        // при ошибках — то, что удалось разобрать; печатать нельзя
  std::vector<Diagnostic> diagnostics;  // лексические и синтаксические, по смещению
  bool ok() const { return diagnostics.empty(); }
};

ParseResult parse(std::string_view source);

// Разбор отдельного фрагмента (для тестов и отладки): весь вход должен быть поглощён.
// При ошибке node == nullptr, diagnostics отсортированы по смещению.
template <class T>
struct FragmentResult {
  T node;
  std::vector<Diagnostic> diagnostics;
};

FragmentResult<ExprPtr> parse_expression(std::string_view source);
FragmentResult<CondPtr> parse_condition(std::string_view source);
FragmentResult<CondPtr> parse_predicate(std::string_view source);

}  // namespace funny

#endif  // FUNNYPARSE_PARSER_H
