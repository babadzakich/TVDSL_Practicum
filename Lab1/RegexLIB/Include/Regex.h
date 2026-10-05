// Разбор регулярных выражений в AST.
//
// Поддерживаемый синтаксис:
//   a|b   ab   a*   a+   a?   (a)   .   [abc]  [a-z]  [^...]
//   экранирование: \n \t \r \f \v \0 \xHH \d \w \s \D \W \S и \<любой не-буквенно-цифровой символ>
// Алфавит — ASCII (0..127); не-ASCII байты в шаблоне считаются ошибкой.

#ifndef FUNNYLEX_REGEX_H
#define FUNNYLEX_REGEX_H

#include <bitset>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace funnylex {

constexpr int kAlphabetSize = 128;
using CharSet = std::bitset<kAlphabetSize>;

class RegexError : public std::runtime_error {
 public:
  RegexError(const std::string& message, size_t position)
      : std::runtime_error(message + " (position " + std::to_string(position) + ")"), position(position) {}
  size_t position;
};

struct RegexNode {
  enum class Kind { EPSILON, CHARSET, CONCAT, ALT, STAR, PLUS, OPTIONAL };

  Kind kind = Kind::EPSILON;
  CharSet chars;
  std::vector<std::unique_ptr<RegexNode>> children;
};

using RegexPtr = std::unique_ptr<RegexNode>;

RegexPtr parse_regex(const std::string& pattern);

bool is_nullable(const RegexNode& node);

}  // namespace funnylex

#endif  // FUNNYLEX_REGEX_H
