#ifndef FUNNYLEX_TEST_UTIL_H
#define FUNNYLEX_TEST_UTIL_H

#include <string>

#include "Dfa.h"
#include "Lexer.h"

namespace funnylex::test {

// Минимальный ДКА для одного регулярного выражения.
inline Dfa compile_single(const std::string& pattern) {
  const RegexPtr ast = parse_regex(pattern);
  return minimize_hopcroft(subset_construction(build_nfa({ast.get()})));
}

inline bool full_match(const std::string& pattern, const std::string& input) {
  return run_dfa(compile_single(pattern), input) == 0;
}

inline const BuildResult& funny_build() {
  static const BuildResult build = build_lexer(funny_spec());
  return build;
}

}  // namespace funnylex::test

#endif  // FUNNYLEX_TEST_UTIL_H
