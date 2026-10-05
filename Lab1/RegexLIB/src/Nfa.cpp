#include "Nfa.h"

#include <algorithm>

namespace funnylex {

namespace {

struct Fragment {
  int start;
  int end;
};

class ThompsonBuilder {
 public:
  explicit ThompsonBuilder(Nfa& nfa) : nfa_(nfa) {}

  int new_state() {
    nfa_.states.emplace_back();
    return nfa_.size() - 1;
  }

  void add_epsilon(int from, int to) { nfa_.states[from].edges.push_back({to, true, {}}); }

  void add_chars(int from, int to, const CharSet& chars) { nfa_.states[from].edges.push_back({to, false, chars}); }

  Fragment build(const RegexNode& node) {
    switch (node.kind) {
      case RegexNode::Kind::EPSILON: {
        const Fragment f{new_state(), new_state()};
        add_epsilon(f.start, f.end);
        return f;
      }
      case RegexNode::Kind::CHARSET: {
        const Fragment f{new_state(), new_state()};
        add_chars(f.start, f.end, node.chars);
        return f;
      }
      case RegexNode::Kind::CONCAT: {
        Fragment result = build(*node.children.front());
        for (size_t i = 1; i < node.children.size(); ++i) {
          const Fragment next = build(*node.children[i]);
          add_epsilon(result.end, next.start);
          result.end = next.end;
        }
        return result;
      }
      case RegexNode::Kind::ALT: {
        const Fragment f{new_state(), -1};
        std::vector<Fragment> branches;
        for (const auto& child : node.children) {
          branches.push_back(build(*child));
        }
        const int end = new_state();
        for (const Fragment& b : branches) {
          add_epsilon(f.start, b.start);
          add_epsilon(b.end, end);
        }
        return {f.start, end};
      }
      case RegexNode::Kind::STAR:
      case RegexNode::Kind::PLUS:
      case RegexNode::Kind::OPTIONAL: {
        const int start = new_state();
        const Fragment inner = build(*node.children.front());
        const int end = new_state();
        add_epsilon(start, inner.start);
        add_epsilon(inner.end, end);
        if (node.kind != RegexNode::Kind::PLUS) {
          add_epsilon(start, end);
        }
        if (node.kind != RegexNode::Kind::OPTIONAL) {
          add_epsilon(inner.end, inner.start);
        }
        return {start, end};
      }
    }
    return {-1, -1};
  }

 private:
  Nfa& nfa_;
};

}  // namespace

Nfa build_nfa(const std::vector<const RegexNode*>& rules) {
  Nfa nfa;
  ThompsonBuilder builder(nfa);
  nfa.start = builder.new_state();
  for (size_t i = 0; i < rules.size(); ++i) {
    const auto [start, end] = builder.build(*rules[i]);
    builder.add_epsilon(nfa.start, start);
    nfa.states[end].accept = static_cast<int>(i);
  }
  return nfa;
}

std::vector<int> epsilon_closure(const Nfa& nfa, std::vector<int> states) {
  std::vector<char> seen(nfa.size(), 0);
  std::vector<int> stack;
  for (int s : states) {
    if (!seen[s]) {
      seen[s] = 1;
      stack.push_back(s);
    }
  }
  states.clear();
  while (!stack.empty()) {
    const int s = stack.back();
    stack.pop_back();
    states.push_back(s);
    for (const NfaEdge& e : nfa.states[s].edges) {
      if (e.epsilon && !seen[e.to]) {
        seen[e.to] = 1;
        stack.push_back(e.to);
      }
    }
  }
  std::ranges::sort(states);
  return states;
}

int simulate_nfa(const Nfa& nfa, std::string_view input) {
  std::vector<int> current = epsilon_closure(nfa, {nfa.start});
  for (const char ch : input) {
    const auto c = static_cast<unsigned char>(ch);
    if (c >= kAlphabetSize) {
      return -1;
    }
    std::vector<int> moved;
    for (int s : current) {
      for (const NfaEdge& e : nfa.states[s].edges) {
        if (!e.epsilon && e.chars.test(c)) {
          moved.push_back(e.to);
        }
      }
    }
    current = epsilon_closure(nfa, std::move(moved));
    if (current.empty()) {
      return -1;
    }
  }
  int best = -1;
  for (int s : current) {
    const int a = nfa.states[s].accept;
    if (a >= 0 && (best < 0 || a < best)) {
      best = a;
    }
  }
  return best;
}

}  // namespace funnylex
