// Список токенов языка Funny: имя + регулярное выражение.

#ifndef FUNNYLEX_TOKEN_SPEC_H
#define FUNNYLEX_TOKEN_SPEC_H

#include <stdexcept>
#include <string>
#include <vector>

namespace funnylex {

class SpecError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct TokenRule {
  std::string name;
  std::string pattern;
  bool skip = false;
  bool error = false;
};

struct TokenSpec {
  std::vector<TokenRule> rules;

  int find(const std::string& name) const;
};

// Токены Funny (вшитый список).
TokenSpec funny_spec();

}  // namespace funnylex

#endif  // FUNNYLEX_TOKEN_SPEC_H
