// Мутации позитивных программ: каждая замена имени или типа даёт какие-то ошибки,
// но анализ не падает, позиции корректны, ошибки отсортированы.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "Sema.h"
#include "Token.h"

using namespace funny;

namespace {

std::vector<std::string> positive_sources() {
  std::vector<std::string> out;
  for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(FUNNY_TESTS_DIR) / "positive")) {
    if (entry.path().extension() == ".funny") {
      std::ifstream in(entry.path(), std::ios::binary);
      std::ostringstream buffer;
      buffer << in.rdbuf();
      out.push_back(buffer.str());
    }
  }
  return out;
}

void check_sane(const std::string& src) {
  const AnalyzeResult r = analyze(src);
  for (std::size_t i = 0; i < r.diagnostics.size(); ++i) {
    const Diagnostic& d = r.diagnostics[i];
    ASSERT_GE(d.pos.line, 1) << src;
    ASSERT_GE(d.pos.col, 1) << src;
    ASSERT_LE(d.pos.offset, src.size()) << src;
    ASSERT_FALSE(d.message.empty());
    if (i > 0) {
      ASSERT_LE(r.diagnostics[i - 1].pos.offset, d.pos.offset) << src;
    }
  }
}

}  // namespace

TEST(Robust, MutatedNamesAndTypes) {
  const std::vector<std::string> sources = positive_sources();
  ASSERT_GE(sources.size(), 10u);
  std::size_t mutants = 0;
  for (const std::string& src : sources) {
    const LexResult lexed = lex(src);
    for (const Token& t : lexed.tokens) {
      std::vector<std::string> replacements;
      if (t.kind == funny_lexer::TokenKind::IDENT) {
        replacements = {"zz", "a", "length", "sum"};
      } else if (t.kind == funny_lexer::TokenKind::KW_INT) {
        replacements = {"int[]"};
      } else if (t.kind == funny_lexer::TokenKind::INT) {
        replacements = {"99999999999"};
      }
      for (const std::string& rep : replacements) {
        std::string mutant = src;
        mutant.replace(t.pos.offset, t.text.size(), rep);
        check_sane(mutant);
        ++mutants;
      }
    }
  }
  EXPECT_GT(mutants, 500u);
}
