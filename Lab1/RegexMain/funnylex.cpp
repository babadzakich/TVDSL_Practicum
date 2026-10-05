#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "Export.h"
#include "Lexer.h"
#include "TestCases.h"

using namespace funnylex;

namespace {

constexpr const char* kUsage = R"(Usage:
  funnylex build [OUT_DIR]      построить автомат, записать OUT_DIR/dfa.json и OUT_DIR/funny_lexer_table.h
                                (по умолчанию OUT_DIR = artifacts)
  funnylex test  [CASES_FILE]   прогнать тесты (по умолчанию tests/cases.txt)
  funnylex lex   [FILE]         разбить FILE (или stdin) на токены
  funnylex match STRING...      прогнать строки целиком через ДКА
)";

std::string read_all(std::istream& in) {
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

void write_file(const std::filesystem::path& path, const std::string& content) {
  std::ofstream out(path, std::ios::binary);
  out << content;
  std::cout << "wrote " << path.string() << "\n";
}

int cmd_build(const BuildResult& b, const std::string& out_dir) {
  std::cout << "tokens:          " << b.spec.rules.size() << "\n"
            << "NFA (Thompson):  " << b.nfa.size() << " states\n"
            << "DFA (subsets):   " << b.dfa.size() << " states (incl. trap)\n"
            << "min DFA:         " << b.min_dfa.size() << " states (trap = " << b.table.trap
            << ", start = " << b.table.start << ")\n";
  std::filesystem::create_directories(out_dir);
  write_file(std::filesystem::path(out_dir) / "dfa.json", export_json(b));
  write_file(std::filesystem::path(out_dir) / "funny_lexer_table.h", export_cpp_header(b));
  return 0;
}

int cmd_test(const BuildResult& b, const std::string& cases_path) {
  const std::vector<TestCase> cases = load_test_cases(cases_path);
  int passed = 0;
  for (const TestCase& test : cases) {
    const TestOutcome outcome = run_test_case(b.table, test);
    passed += outcome.passed ? 1 : 0;
    std::cout << (outcome.passed ? "PASS" : "FAIL") << "  [line " << test.line << "] " << format_test_case(test)
              << "\n";
    if (!outcome.passed) {
      std::cout << "      actual: " << (outcome.actual.empty() ? "(no tokens)" : outcome.actual) << "\n";
    }
  }
  std::cout << "\n" << passed << "/" << cases.size() << " passed\n";
  return passed == static_cast<int>(cases.size()) ? 0 : 1;
}

int cmd_lex(const BuildResult& b, const std::string& path) {
  std::string input;
  if (path.empty()) {
    input = read_all(std::cin);
  } else {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
      throw std::runtime_error("cannot open " + path);
    }
    input = read_all(in);
  }

  int errors = 0;
  for (const Token& t : Lexer(b.table).tokenize(input)) {
    const bool error = is_error_token(b.table, t.kind);
    errors += error ? 1 : 0;
    std::cout << t.offset << "\t" << token_name(b.table, t.kind) << "\t\"" << escape_string(t.lexeme) << "\""
              << (error ? "\t<- lexical error" : "") << "\n";
  }
  return errors > 0 ? 1 : 0;
}

int cmd_match(const BuildResult& b, int argc, char** argv) {
  for (int i = 2; i < argc; ++i) {
    const WholeMatch m = match_whole(b.table, argv[i]);
    std::cout << "\"" << escape_string(argv[i]) << "\"\t";
    switch (m.status) {
      case WholeMatch::Status::ACCEPT: std::cout << b.table.token_names[m.token] << "\n"; break;
      case WholeMatch::Status::REJECT: std::cout << "REJECT\n"; break;
      case WholeMatch::Status::TRAP: std::cout << "TRAP\n"; break;
    }
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << kUsage;
    return 2;
  }
  const std::string command = argv[1];
  const std::string arg = argc > 2 ? argv[2] : "";
  try {
    const BuildResult b = build_lexer(funny_spec());
    if (command == "build") {
      return cmd_build(b, arg.empty() ? "artifacts" : arg);
    }
    if (command == "test") {
      return cmd_test(b, arg.empty() ? "tests/cases.txt" : arg);
    }
    if (command == "lex") {
      return cmd_lex(b, arg);
    }
    if (command == "match") {
      return cmd_match(b, argc, argv);
    }
    std::cerr << kUsage;
    return 2;
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 2;
  }
}
