#include "Lexer.h"

#include <map>

namespace funnylex {

LexerTable make_lexer_table(const Dfa& min_dfa, const TokenSpec& spec) {
  LexerTable table;
  table.start = min_dfa.start;
  table.trap = min_dfa.trap;
  table.accept = min_dfa.accept;
  for (const TokenRule& rule : spec.rules) {
    table.token_names.push_back(rule.name);
    table.skip.push_back(rule.skip);
    table.is_error.push_back(rule.error);
  }

  // Классы байтов: одинаковый столбец переходов -> один класс.
  // Байты 128..255 (вне алфавита) из любого состояния ведут в ловушку.
  const int n = min_dfa.size();
  std::map<std::vector<int>, int> class_of_column;
  for (int byte = 0; byte < 256; ++byte) {
    std::vector<int> column(n);
    for (int s = 0; s < n; ++s) {
      column[s] = byte < kAlphabetSize ? min_dfa.next[s][byte] : min_dfa.trap;
    }
    auto [it, inserted] = class_of_column.try_emplace(std::move(column), table.num_classes);
    if (inserted) {
      ++table.num_classes;
    }
    table.byte_class[byte] = it->second;
  }

  table.next.assign(n, std::vector<int>(table.num_classes));
  for (const auto& [column, cls] : class_of_column) {
    for (int s = 0; s < n; ++s) {
      table.next[s][cls] = column[s];
    }
  }
  return table;
}

BuildResult build_lexer(const TokenSpec& spec) {
  BuildResult result;
  result.spec = spec;

  std::vector<RegexPtr> asts;
  std::vector<const RegexNode*> roots;
  for (const TokenRule& rule : spec.rules) {
    try {
      asts.push_back(parse_regex(rule.pattern));
    } catch (const RegexError& e) {
      throw SpecError("token " + rule.name + ": bad regex '" +
                      rule.pattern + "': " + e.what());
    }
    if (is_nullable(*asts.back())) {
      throw SpecError("token " + rule.name + ": regex '" + rule.pattern +
                      "' matches the empty string");
    }
    roots.push_back(asts.back().get());
  }

  result.nfa = build_nfa(roots);
  result.dfa = subset_construction(result.nfa);
  result.min_dfa = minimize_hopcroft(result.dfa);
  result.table = make_lexer_table(result.min_dfa, spec);
  return result;
}

std::string token_name(const LexerTable& table, int kind) {
  return kind == kErrorToken ? "ERROR" : table.token_names[kind];
}

bool is_error_token(const LexerTable& table, int kind) {
  return kind == kErrorToken || table.is_error[kind];
}

Token Lexer::next(std::string_view input, size_t pos) const {
  int state = table_.start;
  int last_kind = kErrorToken;
  size_t last_end = pos;
  for (size_t i = pos; i < input.size(); ++i) {
    state = table_.step(state, static_cast<unsigned char>(input[i]));
    if (state == table_.trap) {
      break;
    }
    if (table_.accept[state] >= 0) {
      last_kind = table_.accept[state];
      last_end = i + 1;
    }
  }

  if (last_kind != kErrorToken) {
    return {last_kind, std::string(input.substr(pos, last_end - pos)), pos};
  }

  size_t end = pos + 1;
  if (static_cast<unsigned char>(input[pos]) >= 0xC0) {
    while (end < input.size() && (static_cast<unsigned char>(input[end]) & 0xC0) == 0x80) {
      ++end;
    }
  }
  return {kErrorToken, std::string(input.substr(pos, end - pos)), pos};
}

std::vector<Token> Lexer::tokenize(std::string_view input, bool keep_skipped) const {
  std::vector<Token> tokens;
  size_t pos = 0;
  while (pos < input.size()) {
    Token token = next(input, pos);
    pos += token.lexeme.size();
    if (keep_skipped || token.kind == kErrorToken || !table_.skip[token.kind]) {
      tokens.push_back(std::move(token));
    }
  }
  return tokens;
}

WholeMatch match_whole(const LexerTable& table, std::string_view input) {
  int state = table.start;
  for (const char ch : input) {
    state = table.step(state, static_cast<unsigned char>(ch));
    if (state == table.trap) {
      return {WholeMatch::Status::TRAP, -1};
    }
  }
  if (table.accept[state] >= 0) {
    return {WholeMatch::Status::ACCEPT, table.accept[state]};
  }
  return {WholeMatch::Status::REJECT, -1};
}

}  // namespace funnylex
