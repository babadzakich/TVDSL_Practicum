#include "TestCases.h"

#include <fstream>
#include <sstream>

#include "Export.h"

namespace funnylex {

namespace {

class CaseError : public std::runtime_error {
 public:
  CaseError(int line, const std::string& message)
      : std::runtime_error("cases line " + std::to_string(line) + ": " + message) {}
};

void skip_spaces(const std::string& s, size_t& pos) {
  while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\r')) {
    ++pos;
  }
}

int hex_digit(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

std::string parse_quoted(const std::string& s, size_t& pos, int line) {
  if (pos >= s.size() || s[pos] != '"') {
    throw CaseError(line, "expected '\"'");
  }
  ++pos;
  std::string out;
  while (true) {
    if (pos >= s.size()) {
      throw CaseError(line, "unterminated string");
    }
    const char c = s[pos++];
    if (c == '"') {
      return out;
    }
    if (c != '\\') {
      out += c;
      continue;
    }
    if (pos >= s.size()) {
      throw CaseError(line, "dangling '\\'");
    }
    const char e = s[pos++];
    switch (e) {
      case 'n':
        out += '\n';
        break;
      case 't':
        out += '\t';
        break;
      case 'r':
        out += '\r';
        break;
      case '0':
        out += '\0';
        break;
      case '\\':
        out += '\\';
        break;
      case '"':
        out += '"';
        break;
      case 'x': {
        const int hi = pos < s.size() ? hex_digit(s[pos]) : -1;
        const int lo = pos + 1 < s.size() ? hex_digit(s[pos + 1]) : -1;
        if (hi < 0 || lo < 0) {
          throw CaseError(line, "bad \\x escape");
        }
        out += static_cast<char>(hi * 16 + lo);
        pos += 2;
        break;
      }
      default:
        throw CaseError(line, std::string("unknown escape \\") + e);
    }
  }
}

void expect_char(const std::string& s, size_t& pos, char c, int line) {
  skip_spaces(s, pos);
  if (pos >= s.size() || s[pos] != c) {
    throw CaseError(line, std::string("expected '") + c + "'");
  }
  ++pos;
}

std::string format_token(const LexerTable& table, const Token& token) {
  return token_name(table, token.kind) + "=\"" + escape_string(token.lexeme) + "\"";
}

bool item_matches(const std::string& expected, const LexerTable& table, const Token& token) {
  const auto eq = expected.find('=');
  if (eq == std::string::npos) {
    return expected == token_name(table, token.kind);
  }
  return expected == format_token(table, token);
}

}  // namespace

std::vector<TestCase> parse_test_cases(const std::string& text) {
  std::vector<TestCase> cases;
  std::istringstream in(text);
  std::string line;
  int line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    size_t pos = 0;
    skip_spaces(line, pos);
    if (pos >= line.size() || line[pos] == '#') {
      continue;
    }

    TestCase test;
    test.line = line_no;
    const size_t kind_start = pos;
    while (pos < line.size() && line[pos] != ' ' && line[pos] != '\t' && line[pos] != '|') {
      ++pos;
    }
    const std::string kind = line.substr(kind_start, pos - kind_start);
    if (kind == "match") {
      test.kind = TestCase::Kind::MATCH;
    } else if (kind == "lex") {
      test.kind = TestCase::Kind::LEX;
    } else {
      throw CaseError(line_no, "unknown test kind '" + kind + "' (expected match or lex)");
    }

    expect_char(line, pos, '|', line_no);
    skip_spaces(line, pos);
    test.input = parse_quoted(line, pos, line_no);
    expect_char(line, pos, '|', line_no);

    while (true) {
      skip_spaces(line, pos);
      if (pos >= line.size() || line[pos] == '#') {
        break;
      }
      std::string item;
      while (pos < line.size() && line[pos] != ' ' && line[pos] != '\t' && line[pos] != '=') {
        item += line[pos++];
      }
      if (pos < line.size() && line[pos] == '=') {
        ++pos;
        item += "=\"" + escape_string(parse_quoted(line, pos, line_no)) + "\"";
      }
      test.expected.push_back(item);
    }
    if (test.kind == TestCase::Kind::MATCH && test.expected.size() != 1) {
      throw CaseError(line_no, "match test expects exactly one result (TOKEN, REJECT or TRAP)");
    }
    cases.push_back(std::move(test));
  }
  return cases;
}

std::vector<TestCase> load_test_cases(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("cannot open test cases file '" + path + "'");
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return parse_test_cases(buffer.str());
}

TestOutcome run_test_case(const LexerTable& table, const TestCase& test) {
  TestOutcome outcome;
  if (test.kind == TestCase::Kind::MATCH) {
    const WholeMatch m = match_whole(table, test.input);
    switch (m.status) {
      case WholeMatch::Status::ACCEPT:
        outcome.actual = table.token_names[m.token];
        break;
      case WholeMatch::Status::REJECT:
        outcome.actual = "REJECT";
        break;
      case WholeMatch::Status::TRAP:
        outcome.actual = "TRAP";
        break;
    }
    outcome.passed = outcome.actual == test.expected.front();
    return outcome;
  }

  const std::vector<Token> tokens = Lexer(table).tokenize(test.input);
  outcome.passed = tokens.size() == test.expected.size();
  for (size_t i = 0; i < tokens.size(); ++i) {
    outcome.actual += (i ? " " : "") + format_token(table, tokens[i]);
    if (outcome.passed && !item_matches(test.expected[i], table, tokens[i])) {
      outcome.passed = false;
    }
  }
  return outcome;
}

std::string format_test_case(const TestCase& test) {
  std::string out = test.kind == TestCase::Kind::MATCH ? "match" : "lex";
  out += " \"" + escape_string(test.input) + "\" ->";
  for (const std::string& e : test.expected) {
    out += " " + e;
  }
  if (test.expected.empty()) {
    out += " (no tokens)";
  }
  return out;
}

}  // namespace funnylex
