// Построение НКА по Томпсону.

#ifndef FUNNYLEX_NFA_H
#define FUNNYLEX_NFA_H

#include <string_view>
#include <vector>

#include "Regex.h"

namespace funnylex {

struct NfaEdge {
  int to = 0;
  bool epsilon = false;
  CharSet chars;
};

struct NfaState {
  std::vector<NfaEdge> edges;
  int accept = -1;
};

struct Nfa {
  std::vector<NfaState> states;
  int start = 0;

  int size() const { return static_cast<int>(states.size()); }
};

// Общий НКА для списка правил: новый старт с epsilon-переходами в старт каждого
// правила; конечное состояние правила i помечено accept = i.
Nfa build_nfa(const std::vector<const RegexNode*>& rules);

// epsilon-замыкание множества состояний (результат отсортирован).
std::vector<int> epsilon_closure(const Nfa& nfa, std::vector<int> states);

// Прямая симуляция НКА по всей строке: номер правила с наивысшим приоритетом
// (наименьший номер), принимающего строку целиком, или -1.
int simulate_nfa(const Nfa& nfa, std::string_view input);

}  // namespace funnylex

#endif  // FUNNYLEX_NFA_H
