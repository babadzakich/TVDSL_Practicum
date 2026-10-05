// ДКА: построение подмножеств и минимизация Хопкрофта.

#ifndef FUNNYLEX_DFA_H
#define FUNNYLEX_DFA_H

#include <array>
#include <string_view>
#include <vector>

#include "Nfa.h"

namespace funnylex {

struct Dfa {
  std::vector<std::array<int, kAlphabetSize>> next;
  std::vector<int> accept;
  int start = 0;
  int trap = -1;

  int size() const { return static_cast<int>(next.size()); }
};

Dfa subset_construction(const Nfa& nfa);

Dfa minimize_hopcroft(const Dfa& dfa);

int run_dfa(const Dfa& dfa, std::string_view input);

}  // namespace funnylex

#endif  // FUNNYLEX_DFA_H
