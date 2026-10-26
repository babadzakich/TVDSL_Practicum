#ifndef FUNNYPARSE_PARSER_H
#define FUNNYPARSE_PARSER_H

#include <string_view>
#include <vector>

#include "Ast.h"
#include "Diagnostic.h"

namespace funny {

inline constexpr int kMaxDepth = 1000;

struct ParseResult {
  Module module;                        
  std::vector<Diagnostic> diagnostics;  
  bool ok() const { return diagnostics.empty(); }
};

ParseResult parse(std::string_view source);

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
