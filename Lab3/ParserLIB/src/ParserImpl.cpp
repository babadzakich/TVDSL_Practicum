#include "ParserImpl.h"

#include <algorithm>

namespace funny {

namespace {

std::string join_expected(const std::vector<std::string>& items) {
  std::string out;
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i > 0) {
      out += i + 1 == items.size() ? " или " : ", ";
    }
    out += items[i];
  }
  return out;
}

}  // namespace

ParserImpl::ParserImpl(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

const Token& ParserImpl::peek(std::size_t ahead) const {
  return tokens_[std::min(pos_ + ahead, tokens_.size() - 1)];
}

bool ParserImpl::at(TokenKind k, std::size_t ahead) const { return peek(ahead).kind == k; }

void ParserImpl::advance() {
  if (pos_ + 1 < tokens_.size()) {
    ++pos_;
  }
}

bool ParserImpl::accept(TokenKind k) {
  if (!at(k)) {
    return false;
  }
  advance();
  return true;
}

bool ParserImpl::expect(TokenKind k) {
  if (accept(k)) {
    return true;
  }
  fail(describe_kind(k));
  return false;
}

void ParserImpl::add_expected(std::string what) {
  if (std::ranges::find(expected_, what) == expected_.end()) {
    expected_.push_back(std::move(what));
  }
}

void ParserImpl::fail(std::string what) {
  if (pos_ > far_) {
    far_ = pos_;
    expected_.clear();
  }
  if (pos_ == far_) {
    add_expected(std::move(what));
  }
}

void ParserImpl::restore_failures(FailureState s) {
  far_ = s.far;
  expected_ = std::move(s.expected);
}

std::optional<Diagnostic> ParserImpl::farthest_diagnostic() const {
  const Token& t = tokens_[std::min(far_, tokens_.size() - 1)];
  if (expected_.empty() || t.kind == TokenKind::ERROR) {
    return std::nullopt;
  }
  std::string message = "ожидается " + join_expected(expected_);
  message += t.kind == kEof ? ", найден конец файла" : ", найдено " + quote_lexeme(t.text);
  return Diagnostic{t.pos, std::move(message)};
}

void ParserImpl::clear_failures() {
  far_ = 0;
  expected_.clear();
  expr_memo_.clear();
}

ParserImpl::DepthGuard::DepthGuard(ParserImpl& p) : p_(p) {
  if (++p_.depth_ > kMaxDepth) {
    --p_.depth_;
    throw DepthExceeded{p_.peek().pos};
  }
}

void ParserImpl::ChainDepth::add() {
  ++links_;
  if (++p_.depth_ > kMaxDepth) {
    throw DepthExceeded{p_.peek().pos};
  }
}

}  // namespace funny
