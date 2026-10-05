#include <gtest/gtest.h>

#include <map>
#include <queue>
#include <random>
#include <set>

#include "TestUtil.h"
#include "Dfa.h"
#include "Nfa.h"

using namespace funnylex;
using funnylex::test::compile_single;
using funnylex::test::funny_build;

namespace {

// Независимая эталонная минимизация (алгоритм Мура, итеративное уточнение классов)
// для перекрёстной проверки Хопкрофта: возвращает число классов эквивалентности.
int moore_state_count(const Dfa& dfa) {
  const int n = dfa.size();
  std::vector<int> cls(n);
  {
    std::map<int, int> by_token;
    for (int s = 0; s < n; ++s) {
      cls[s] = by_token.try_emplace(dfa.accept[s], static_cast<int>(by_token.size())).first->second;
    }
  }
  int count = 0;
  while (true) {
    std::map<std::vector<int>, int> signatures;
    std::vector<int> next_cls(n);
    for (int s = 0; s < n; ++s) {
      std::vector<int> sig{cls[s]};
      for (int c = 0; c < kAlphabetSize; ++c) {
        sig.push_back(cls[dfa.next[s][c]]);
      }
      next_cls[s] = signatures.try_emplace(sig, static_cast<int>(signatures.size())).first->second;
    }
    const int new_count = static_cast<int>(signatures.size());
    cls = std::move(next_cls);
    if (new_count == count) {
      return count;
    }
    count = new_count;
  }
}

std::set<int> reachable(const Dfa& dfa) {
  std::set<int> seen{dfa.start};
  std::queue<int> q;
  q.push(dfa.start);
  while (!q.empty()) {
    const int s = q.front();
    q.pop();
    for (int c = 0; c < kAlphabetSize; ++c) {
      if (seen.insert(dfa.next[s][c]).second) {
        q.push(dfa.next[s][c]);
      }
    }
  }
  return seen;
}

void expect_well_formed(const Dfa& dfa) {
  ASSERT_GE(dfa.trap, 0);
  EXPECT_EQ(dfa.accept[dfa.trap], -1);
  for (int c = 0; c < kAlphabetSize; ++c) {
    EXPECT_EQ(dfa.next[dfa.trap][c], dfa.trap);
  }
  EXPECT_EQ(static_cast<int>(reachable(dfa).size()), dfa.size());  // лишних состояний нет
}

}  // namespace

TEST(Automata, ThompsonShape) {
  // Томпсон: символ — 2 состояния, конкатенация суммирует, '|' и '*' добавляют по 2.
  const RegexPtr a = parse_regex("a");
  EXPECT_EQ(build_nfa({a.get()}).size(), 1 + 2);  // + общий старт
  const RegexPtr ab_star = parse_regex("(a|b)*");
  EXPECT_EQ(build_nfa({ab_star.get()}).size(), 1 + 2 + 2 + 2 + 2);
}

TEST(Automata, ClassicTextbookExample) {
  // (a|b)*abb: минимальный ДКА из учебника — 4 состояния, плюс ловушка для прочих символов.
  const Dfa min = compile_single("(a|b)*abb");
  EXPECT_EQ(min.size(), 5);
  EXPECT_EQ(min.trap, 0);
  EXPECT_EQ(min.start, 1);
  EXPECT_EQ(run_dfa(min, "abb"), 0);
  EXPECT_EQ(run_dfa(min, "babaabb"), 0);
  EXPECT_EQ(run_dfa(min, "abba"), -1);
  EXPECT_EQ(run_dfa(min, "abc"), -1);
}

TEST(Automata, EquivalentRegexesMinimizeToSameSize) {
  EXPECT_EQ(compile_single("a*").size(), 2);
  EXPECT_EQ(compile_single("(a|b)*").size(), compile_single("(a*b*)*").size());
  EXPECT_EQ(compile_single("aa*").size(), compile_single("a+").size());
  EXPECT_EQ(compile_single("a(ba)*").size(), compile_single("(ab)*a").size());
  EXPECT_EQ(compile_single("x|x|x").size(), compile_single("x").size());
}

TEST(Automata, SubsetConstructionTrapIsEmptySet) {
  const Dfa dfa = funny_build().dfa;
  expect_well_formed(dfa);
  // В ловушку ведёт, например, '@' из старта.
  EXPECT_EQ(dfa.next[dfa.start]['@'], dfa.trap);
}

TEST(Automata, MinimizedIsWellFormedAndMinimal) {
  const BuildResult& b = funny_build();
  expect_well_formed(b.min_dfa);
  EXPECT_EQ(b.min_dfa.trap, 0);
  EXPECT_EQ(b.min_dfa.start, 1);
  EXPECT_LE(b.min_dfa.size(), b.dfa.size());
  // Все состояния достижимы и попарно различимы (эталонный Мур не может склеить больше).
  EXPECT_EQ(static_cast<int>(reachable(b.min_dfa).size()), b.min_dfa.size());
  EXPECT_EQ(moore_state_count(b.min_dfa), b.min_dfa.size());
  EXPECT_EQ(moore_state_count(b.dfa), b.min_dfa.size());
}

TEST(Automata, HopcroftMatchesMooreOnVariousRegexes) {
  for (const std::string pattern : {"(a|b)*abb", "a*b*c*", "(ab|ba)*", "[0-9]+(\\.[0-9]+)?", "(a|b)*a(a|b)(a|b)",
                                    "//[^\\n]*", "0|[1-9][0-9]*", "(x|y|z)*xyz(x|y)*"}) {
    const RegexPtr ast = parse_regex(pattern);
    const Dfa dfa = subset_construction(build_nfa({ast.get()}));
    EXPECT_EQ(minimize_hopcroft(dfa).size(), moore_state_count(dfa)) << pattern;
  }
}

TEST(Automata, NfaDfaMinDfaAgreeExhaustively) {
  // Все строки длины <= 4 над "интересным" алфавитом.
  const BuildResult& b = funny_build();
  const std::string alphabet = "if0_1=!/ \n\xFF";
  std::vector<std::string> layer{""};
  for (int len = 0; len <= 4; ++len) {
    std::vector<std::string> next_layer;
    for (const std::string& s : layer) {
      const int expected = simulate_nfa(b.nfa, s);
      ASSERT_EQ(run_dfa(b.dfa, s), expected) << '"' << s << '"';
      ASSERT_EQ(run_dfa(b.min_dfa, s), expected) << '"' << s << '"';
      for (const char c : alphabet) {
        next_layer.push_back(s + c);
      }
    }
    layer = std::move(next_layer);
  }
}

TEST(Automata, NfaDfaMinDfaAgreeOnRandomStrings) {
  const BuildResult& b = funny_build();
  // Смесь фрагментов токенов, чтобы случайные строки часто оказывались токенами.
  const std::vector<std::string> pieces = {"a",  "z",  "_", "0", "1", "9",    "if",       "while", "int",
                                           "=",  "<",  ">", "!", "-", "/",    "//",       " ",     "\t",
                                           "\r", "\n", "(", "]", "@", "\x80", "\xD0\xB6", "~",     "length"};
  std::mt19937 rng(12345);
  std::uniform_int_distribution<size_t> pick(0, pieces.size() - 1);
  std::uniform_int_distribution<int> length(1, 6);
  for (int iter = 0; iter < 20000; ++iter) {
    std::string s;
    for (int k = length(rng); k > 0; --k) {
      s += pieces[pick(rng)];
    }
    const int expected = simulate_nfa(b.nfa, s);
    ASSERT_EQ(run_dfa(b.dfa, s), expected) << '"' << s << '"';
    ASSERT_EQ(run_dfa(b.min_dfa, s), expected) << '"' << s << '"';
  }
}

TEST(Automata, PriorityKeywordOverIdent) {
  const BuildResult& b = funny_build();
  EXPECT_EQ(run_dfa(b.min_dfa, "while"), b.spec.find("KW_WHILE"));
  EXPECT_EQ(run_dfa(b.min_dfa, "whilex"), b.spec.find("IDENT"));
}

TEST(TokenSpecTest, RejectsBadRegexes) {
  EXPECT_THROW(build_lexer({{{"A", "a*"}}}), SpecError);  // принимает пустую строку
  EXPECT_THROW(build_lexer({{{"A", "(a"}}}), SpecError);
  EXPECT_NO_THROW(build_lexer({{{"A", "a+"}}}));
}
