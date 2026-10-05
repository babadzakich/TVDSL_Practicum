#include "TokenSpec.h"

namespace funnylex {

int TokenSpec::find(const std::string& name) const {
  for (size_t i = 0; i < rules.size(); ++i) {
    if (rules[i].name == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

TokenSpec funny_spec() {
  const bool skip = true;
  const bool error = true;
  return {{
      {"KW_FUNCTION", "function"},
      {"KW_RETURNS", "returns"},
      {"KW_REQUIRES", "requires"},
      {"KW_ENSURES", "ensures"},
      {"KW_USES", "uses"},
      {"KW_WHILE", "while"},
      {"KW_IF", "if"},
      {"KW_ELSE", "else"},
      {"KW_ASSERT", "assert"},
      {"KW_ASSUME", "assume"},
      {"KW_INVARIANT", "invariant"},
      {"KW_LENGTH", "length"},
      {"KW_INT", "int"},
      {"KW_TRUE", "true"},
      {"KW_FALSE", "false"},
      {"KW_NOT", "not"},
      {"KW_AND", "and"},
      {"KW_OR", "or"},
      {"KW_FORALL", "forall"},
      {"KW_EXISTS", "exists"},

      {"IDENT", "[A-Za-z][A-Za-z0-9_]*"},
      {"INT", "0|[1-9][0-9]*"},

      {"BAD_INT", "0[0-9]+", false, error},

      {"WS", "[ \\t\\r\\n]+", skip},
      {"COMMENT", "//[^\\n]*", skip},

      {"EQ", "=="},
      {"NE", "!="},
      {"LE", "<="},
      {"GE", ">="},
      {"ARROW", "->"},
      {"FAT_ARROW", "=>"},

      {"LT", "<"},
      {"GT", ">"},
      {"ASSIGN", "="},
      {"PLUS", "\\+"},
      {"MINUS", "-"},
      {"STAR", "\\*"},
      {"SLASH", "/"},
      {"LPAREN", "\\("},
      {"RPAREN", "\\)"},
      {"LBRACKET", "\\["},
      {"RBRACKET", "\\]"},
      {"LBRACE", "{"},
      {"RBRACE", "}"},
      {"COMMA", ","},
      {"SEMICOLON", ";"},
      {"COLON", ":"},
      {"PIPE", "\\|"},
  }};
}

}  // namespace funnylex
