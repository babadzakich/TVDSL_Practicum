// Парсер обязан завершаться на любом входе и давать осмысленные позиции.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "Parser.h"
#include "Token.h"

using namespace funny;

namespace {

void check_invariants(std::string_view src) {
  const ParseResult r = parse(src);
  const std::size_t tokens = lex(src).tokens.size();
  EXPECT_LE(r.diagnostics.size(), tokens + 1) << src;
  for (const Diagnostic& d : r.diagnostics) {
    EXPECT_GE(d.pos.line, 1) << src;
    EXPECT_GE(d.pos.col, 1) << src;
    EXPECT_LE(d.pos.offset, src.size()) << src;
  }
  EXPECT_TRUE(std::ranges::is_sorted(r.diagnostics, {}, [](const Diagnostic& d) { return d.pos.offset; })) << src;
}

std::vector<std::string> positive_sources() {
  std::vector<std::string> out;
  for (const auto& entry : std::filesystem::directory_iterator(FUNNY_TESTS_DIR "/positive")) {
    if (entry.path().extension() == ".funny") {
      std::ifstream in(entry.path(), std::ios::binary);
      std::ostringstream buffer;
      buffer << in.rdbuf();
      out.push_back(buffer.str());
    }
  }
  return out;
}

void every_token_prefix() {
  const std::vector<std::string> sources = positive_sources();
  ASSERT_FALSE(sources.empty());
  for (const std::string& src : sources) {
    for (const Token& t : lex(src).tokens) {
      check_invariants(std::string_view(src).substr(0, t.pos.offset));
    }
  }
}

void random_token_sequences() {
  static constexpr std::array<const char*, 40> kPool = {
      "f",       "x",        "a",      "(",      ")",      "{",       "}",      "[",      "]",    ";",
      ",",       ":",        "=",      "==",     "<",      "+",       "-",      "*",      "if",   "else",
      "while",   "returns",  "requires", "ensures", "uses", "invariant", "assert", "assume", "forall", "|",
      "=>",      "->",       "and",    "or",     "not",    "int",     "true",   "1",      "007",  "@"};
  std::mt19937 rng(12345);
  std::uniform_int_distribution<std::size_t> length(1, 60);
  std::uniform_int_distribution<std::size_t> pick(0, kPool.size() - 1);
  for (int i = 0; i < 500; ++i) {
    std::string src;
    for (std::size_t n = length(rng); n > 0; --n) {
      src += kPool[pick(rng)];
      src += ' ';
    }
    check_invariants(src);
  }
}

}  // namespace

TEST(Fuzz, EveryTokenPrefixOfPositiveCorpus) { every_token_prefix(); }

TEST(Fuzz, RandomTokenSequences) { random_token_sequences(); }

TEST(Fuzz, FinishesQuickly) {
  const auto start = std::chrono::steady_clock::now();
  every_token_prefix();
  random_token_sequences();
  EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(10));
}
