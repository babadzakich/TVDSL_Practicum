#include "Regex.h"

#include <cctype>

namespace funnylex {

namespace {

RegexPtr make_node(RegexNode::Kind kind) {
  auto node = std::make_unique<RegexNode>();
  node->kind = kind;
  return node;
}

RegexPtr make_charset(const CharSet& chars) {
  auto node = make_node(RegexNode::Kind::CHARSET);
  node->chars = chars;
  return node;
}

CharSet range(int from, int to) {
  CharSet set;
  for (int c = from; c <= to; ++c) {
    set.set(c);
  }
  return set;
}

CharSet digits() {
  return range('0', '9');
}
CharSet word_chars() {
  return range('a', 'z') | range('A', 'Z') | digits() | range('_', '_');
}
CharSet space_chars() {
  CharSet set;
  for (char c : {' ', '\t', '\n', '\r', '\f', '\v'}) {
    set.set(static_cast<unsigned char>(c));
  }
  return set;
}

int hex_value(char c) {
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

class Parser {
 public:
  explicit Parser(const std::string& pattern) : pattern_(pattern) {}

  RegexPtr parse() {
    for (size_t i = 0; i < pattern_.size(); ++i) {
      if (static_cast<unsigned char>(pattern_[i]) >= kAlphabetSize) {
        throw RegexError("non-ASCII byte in pattern", i);
      }
    }
    auto node = parse_alt();
    if (!at_end()) {
      throw RegexError(std::string("unexpected '") + peek() + "'", pos_);
    }
    return node;
  }

 private:
  bool at_end() const { return pos_ >= pattern_.size(); }
  char peek() const { return pattern_[pos_]; }

  RegexPtr parse_alt() {
    std::vector<RegexPtr> branches;
    branches.push_back(parse_concat());
    while (!at_end() && peek() == '|') {
      ++pos_;
      branches.push_back(parse_concat());
    }
    if (branches.size() == 1) {
      return std::move(branches.front());
    }
    auto node = make_node(RegexNode::Kind::ALT);
    node->children = std::move(branches);
    return node;
  }

  RegexPtr parse_concat() {
    std::vector<RegexPtr> items;
    while (!at_end() && peek() != '|' && peek() != ')') {
      items.push_back(parse_repeat());
    }
    if (items.empty()) {
      return make_node(RegexNode::Kind::EPSILON);
    }
    if (items.size() == 1) {
      return std::move(items.front());
    }
    auto node = make_node(RegexNode::Kind::CONCAT);
    node->children = std::move(items);
    return node;
  }

  RegexPtr parse_repeat() {
    auto node = parse_atom();
    while (!at_end() && (peek() == '*' || peek() == '+' || peek() == '?')) {
      const char op = pattern_[pos_++];
      const auto kind = op == '*'   ? RegexNode::Kind::STAR
                        : op == '+' ? RegexNode::Kind::PLUS
                                    : RegexNode::Kind::OPTIONAL;
      auto wrapper = make_node(kind);
      wrapper->children.push_back(std::move(node));
      node = std::move(wrapper);
    }
    return node;
  }

  RegexPtr parse_atom() {
    const size_t start = pos_;
    const char c = pattern_[pos_++];
    switch (c) {
      case '(': {
        auto inner = parse_alt();
        if (at_end() || peek() != ')') {
          throw RegexError("unclosed '('", start);
        }
        ++pos_;
        return inner;
      }
      case '[':
        return make_charset(parse_class(start));
      case '.': {
        CharSet any;
        any.set();
        any.reset('\n');
        return make_charset(any);
      }
      case '\\':
        return make_charset(parse_escape());
      case '*':
      case '+':
      case '?':
        throw RegexError(std::string("nothing to repeat before '") + c + "'", start);
      default: {
        CharSet single;
        single.set(static_cast<unsigned char>(c));
        return make_charset(single);
      }
    }
  }

  CharSet parse_escape() {
    if (at_end()) {
      throw RegexError("dangling '\\'", pos_ - 1);
    }
    const size_t start = pos_ - 1;
    const char c = pattern_[pos_++];
    CharSet set;
    switch (c) {
      case 'n':
        set.set('\n');
        return set;
      case 't':
        set.set('\t');
        return set;
      case 'r':
        set.set('\r');
        return set;
      case 'f':
        set.set('\f');
        return set;
      case 'v':
        set.set('\v');
        return set;
      case '0':
        set.set(0);
        return set;
      case 'd':
        return digits();
      case 'D':
        return ~digits();
      case 'w':
        return word_chars();
      case 'W':
        return ~word_chars();
      case 's':
        return space_chars();
      case 'S':
        return ~space_chars();
      case 'x': {
        const int hi = pos_ < pattern_.size() ? hex_value(pattern_[pos_]) : -1;
        const int lo = pos_ + 1 < pattern_.size() ? hex_value(pattern_[pos_ + 1]) : -1;
        if (hi < 0 || lo < 0) {
          throw RegexError("expected two hex digits after \\x", start);
        }
        pos_ += 2;
        const int value = hi * 16 + lo;
        if (value >= kAlphabetSize) {
          throw RegexError("\\x value outside ASCII alphabet", start);
        }
        set.set(value);
        return set;
      }
      default:
        if (std::isalnum(static_cast<unsigned char>(c))) {
          throw RegexError(std::string("unknown escape \\") + c, start);
        }
        set.set(static_cast<unsigned char>(c));
        return set;
    }
  }

  CharSet parse_class(size_t start) {
    bool negated = false;
    if (!at_end() && peek() == '^') {
      negated = true;
      ++pos_;
    }
    CharSet set;
    bool first = true;
    while (true) {
      if (at_end()) {
        throw RegexError("unclosed '['", start);
      }
      if (peek() == ']' && !first) {
        ++pos_;
        break;
      }
      first = false;

      const size_t item_pos = pos_;
      CharSet item;
      int single = -1;
      if (peek() == '\\') {
        ++pos_;
        item = parse_escape();
        if (item.count() == 1) {
          for (int ch = 0; ch < kAlphabetSize; ++ch) {
            if (item.test(ch)) {
              single = ch;
            }
          }
        }
      } else {
        single = static_cast<unsigned char>(pattern_[pos_++]);
        item.set(single);
      }

      const bool is_range = single >= 0 && pos_ + 1 < pattern_.size() && peek() == '-' && pattern_[pos_ + 1] != ']';
      if (is_range) {
        ++pos_;  // '-'
        int upper = -1;
        if (peek() == '\\') {
          ++pos_;
          const CharSet hi = parse_escape();
          if (hi.count() == 1) {
            for (int ch = 0; ch < kAlphabetSize; ++ch) {
              if (hi.test(ch)) {
                upper = ch;
              }
            }
          }
        } else {
          upper = static_cast<unsigned char>(pattern_[pos_++]);
        }
        if (upper < 0) {
          throw RegexError("invalid range end", item_pos);
        }
        if (upper < single) {
          throw RegexError("reversed range", item_pos);
        }
        item = range(single, upper);
      }
      set |= item;
    }
    return negated ? ~set : set;
  }

  const std::string& pattern_;
  size_t pos_ = 0;
};

}  // namespace

RegexPtr parse_regex(const std::string& pattern) {
  return Parser(pattern).parse();
}

bool is_nullable(const RegexNode& node) {
  switch (node.kind) {
    case RegexNode::Kind::EPSILON:
    case RegexNode::Kind::STAR:
    case RegexNode::Kind::OPTIONAL:
      return true;
    case RegexNode::Kind::CHARSET:
      return false;
    case RegexNode::Kind::PLUS:
      return is_nullable(*node.children.front());
    case RegexNode::Kind::CONCAT:
      for (const auto& child : node.children) {
        if (!is_nullable(*child)) {
          return false;
        }
      }
      return true;
    case RegexNode::Kind::ALT:
      for (const auto& child : node.children) {
        if (is_nullable(*child)) {
          return true;
        }
      }
      return false;
  }
  return false;
}

}  // namespace funnylex
