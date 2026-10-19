#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "AstPrinter.h"
#include "Diagnostic.h"
#include "Parser.h"
#include "Snapshot.h"
#include "Token.h"

using namespace funny;

namespace {

constexpr const char* kUsage = R"(Usage:
  funnyparse [--json] [FILE]        разобрать FILE (или stdin): AST в stdout, ошибки в stderr
  funnyparse tokens [FILE]          поток токенов FILE (или stdin) с позициями
  funnyparse test [--bless] [DIR]   прогнать эталоны DIR/positive и DIR/negative (по умолчанию DIR = tests);
                                    --bless записывает фактический вывод в эталоны
Коды выхода: 0 — успех, 1 — ошибки в программе / проваленные тесты, 2 — ошибка использования или ввода-вывода.
)";

std::optional<std::string> read_input(const std::string& path) {
  std::ostringstream buffer;
  if (path.empty()) {
    buffer << std::cin.rdbuf();
    return buffer.str();
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return std::nullopt;
  }
  buffer << in.rdbuf();
  return buffer.str();
}

std::string label_for(const std::string& path) { return path.empty() ? "<stdin>" : path; }

int cannot_open(const std::string& path) {
  std::cerr << "не удалось открыть '" << path << "'\n";
  return 2;
}

int cmd_parse(const std::string& path, bool json) {
  const std::optional<std::string> src = read_input(path);
  if (!src) {
    return cannot_open(path);
  }
  const ParseResult r = parse(*src);
  if (!r.ok()) {
    std::cerr << render_diagnostics(label_for(path), r.diagnostics);
    return 1;
  }
  std::cout << (json ? print_json(r.module) : print_tree(r.module));
  return 0;
}

int cmd_tokens(const std::string& path) {
  const std::optional<std::string> src = read_input(path);
  if (!src) {
    return cannot_open(path);
  }
  const LexResult r = lex(*src);
  for (const Token& t : r.tokens) {
    std::cout << t.pos.line << ':' << t.pos.col << ' ';
    if (t.kind == kEof) {
      std::cout << "EOF\n";
    } else {
      std::cout << funny_lexer::token_name(t.kind) << ' ' << quote_lexeme(t.text) << '\n';
    }
  }
  std::cerr << render_diagnostics(label_for(path), r.diagnostics);
  return r.diagnostics.empty() ? 0 : 1;
}

int cmd_test(const std::string& dir, bool bless) {
  const std::vector<SnapshotResult> results = run_snapshots(dir, bless);
  std::size_t passed = 0;
  for (const SnapshotResult& r : results) {
    passed += r.passed ? 1 : 0;
    std::cout << (r.passed ? "PASS" : "FAIL") << "  " << r.source.string() << " (" << r.kind << ")\n";
    if (!r.passed) {
      std::cout << "--- ожидалось:\n" << r.expected << "--- получено:\n" << r.actual;
    }
  }
  std::cout << "\n" << passed << "/" << results.size() << " passed\n";
  if (results.empty()) {
    std::cerr << "в '" << dir << "' нет тестов\n";
    return 2;
  }
  return passed == results.size() ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> args(argv + 1, argv + argc);
  if (!args.empty() && args[0] == "tokens" && args.size() <= 2) {
    return cmd_tokens(args.size() == 2 ? args[1] : "");
  }
  if (!args.empty() && args[0] == "test") {
    bool bless = false;
    std::string dir = "tests";
    for (std::size_t i = 1; i < args.size(); ++i) {
      if (args[i] == "--bless") {
        bless = true;
      } else {
        dir = args[i];
      }
    }
    return cmd_test(dir, bless);
  }
  bool json = false;
  std::string path;
  for (const std::string& a : args) {
    if (a == "--json") {
      json = true;
    } else if (a == "-h" || a == "--help" || !path.empty()) {
      std::cerr << kUsage;
      return 2;
    } else {
      path = a;
    }
  }
  return cmd_parse(path, json);
}
