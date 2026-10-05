#include "Dfa.h"

#include <algorithm>
#include <map>
#include <queue>

namespace funnylex {

Dfa subset_construction(const Nfa& nfa) {
  Dfa dfa;
  std::map<std::vector<int>, int> index;
  std::vector<std::vector<int>> sets;

  auto intern = [&](std::vector<int> set) {
    auto [it, inserted] = index.try_emplace(set, static_cast<int>(sets.size()));
    if (inserted) {
      int accept = -1;
      for (int s : set) {
        const int a = nfa.states[s].accept;
        if (a >= 0 && (accept < 0 || a < accept)) {
          accept = a;
        }
      }
      if (set.empty()) {
        dfa.trap = it->second;
      }
      sets.push_back(std::move(set));
      dfa.next.emplace_back();
      dfa.accept.push_back(accept);
    }
    return it->second;
  };

  dfa.start = intern(epsilon_closure(nfa, {nfa.start}));
  for (size_t current = 0; current < sets.size(); ++current) {
    for (int c = 0; c < kAlphabetSize; ++c) {
      std::vector<int> moved;
      for (int s : sets[current]) {
        for (const NfaEdge& e : nfa.states[s].edges) {
          if (!e.epsilon && e.chars.test(c)) {
            moved.push_back(e.to);
          }
        }
      }
      const int target = intern(epsilon_closure(nfa, std::move(moved)));
      dfa.next[current][c] = target;
    }
  }

  if (dfa.trap < 0) {
    dfa.trap = dfa.size();
    dfa.next.emplace_back();
    dfa.next.back().fill(dfa.trap);
    dfa.accept.push_back(-1);
  }
  return dfa;
}

Dfa minimize_hopcroft(const Dfa& dfa) {
  const int n = dfa.size();

  std::vector<std::array<std::vector<int>, kAlphabetSize>> inverse(n);
  for (int s = 0; s < n; ++s) {
    for (int c = 0; c < kAlphabetSize; ++c) {
      inverse[dfa.next[s][c]][c].push_back(s);
    }
  }

  std::vector<std::vector<int>> blocks;
  std::vector<int> block_of(n);
  {
    std::map<int, std::vector<int>> by_token;
    for (int s = 0; s < n; ++s) {
      by_token[dfa.accept[s]].push_back(s);
    }
    for (auto& [token, members] : by_token) {
      for (int s : members) {
        block_of[s] = static_cast<int>(blocks.size());
      }
      blocks.push_back(std::move(members));
    }
  }

  std::vector<int> work;
  std::vector<char> in_work(blocks.size(), 0);
  {
    int largest = 0;
    for (int b = 1; b < static_cast<int>(blocks.size()); ++b) {
      if (blocks[b].size() > blocks[largest].size()) {
        largest = b;
      }
    }
    for (int b = 0; b < static_cast<int>(blocks.size()); ++b) {
      if (b != largest) {
        work.push_back(b);
        in_work[b] = 1;
      }
    }
  }

  std::vector<char> marked(n, 0);
  while (!work.empty()) {
    const int splitter_id = work.back();
    work.pop_back();
    in_work[splitter_id] = 0;
    const std::vector<int> splitter = blocks[splitter_id];

    for (int c = 0; c < kAlphabetSize; ++c) {
      std::map<int, std::vector<int>> hit;
      for (int t : splitter) {
        for (int s : inverse[t][c]) {
          hit[block_of[s]].push_back(s);
        }
      }
      for (auto& [y, inside] : hit) {
        if (inside.size() == blocks[y].size()) {
          continue;
        }
        for (int s : inside) {
          marked[s] = 1;
        }
        std::vector<int> outside;
        for (int s : blocks[y]) {
          if (!marked[s]) {
            outside.push_back(s);
          }
        }
        for (int s : inside) {
          marked[s] = 0;
        }

        const int z = static_cast<int>(blocks.size());
        blocks[y] = std::move(inside);
        for (int s : outside) {
          block_of[s] = z;
        }
        blocks.push_back(std::move(outside));
        in_work.push_back(0);

        if (in_work[y]) {
          work.push_back(z);
          in_work[z] = 1;
        } else {
          const int smaller = blocks[y].size() <= blocks[z].size() ? y : z;
          work.push_back(smaller);
          in_work[smaller] = 1;
        }
      }
    }
  }

  std::vector<int> new_id(blocks.size(), -1);
  std::vector<int> order;
  auto assign = [&](int block) {
    if (new_id[block] < 0) {
      new_id[block] = static_cast<int>(order.size());
      order.push_back(block);
    }
  };
  assign(block_of[dfa.trap]);
  assign(block_of[dfa.start]);
  for (size_t i = 0; i < order.size(); ++i) {
    const int representative = blocks[order[i]].front();
    for (int c = 0; c < kAlphabetSize; ++c) {
      assign(block_of[dfa.next[representative][c]]);
    }
  }

  Dfa result;
  result.trap = 0;
  result.start = new_id[block_of[dfa.start]];
  result.next.resize(order.size());
  result.accept.resize(order.size());
  for (size_t i = 0; i < order.size(); ++i) {
    const int representative = blocks[order[i]].front();
    result.accept[i] = dfa.accept[representative];
    for (int c = 0; c < kAlphabetSize; ++c) {
      result.next[i][c] = new_id[block_of[dfa.next[representative][c]]];
    }
  }
  return result;
}

int run_dfa(const Dfa& dfa, std::string_view input) {
  int state = dfa.start;
  for (const char ch : input) {
    const auto c = static_cast<unsigned char>(ch);
    state = c < kAlphabetSize ? dfa.next[state][c] : dfa.trap;
    if (state == dfa.trap) {
      return -1;
    }
  }
  return dfa.accept[state];
}

}  // namespace funnylex
