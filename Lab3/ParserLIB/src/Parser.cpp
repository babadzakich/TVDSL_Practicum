#include "Parser.h"

#include "ParserImpl.h"

namespace funny {

namespace {

template <class T, class Rule>
FragmentResult<T> parse_fragment(std::string_view source, Rule rule) {
  LexResult lexed = lex(source);
  FragmentResult<T> r{nullptr, std::move(lexed.diagnostics)};
  ParserImpl p(std::move(lexed.tokens));
  try {
    r.node = rule(p);
    if (!r.node || !p.expect(kEof)) {
      r.node = nullptr;
      if (auto d = p.farthest_diagnostic()) {
        r.diagnostics.push_back(std::move(*d));
      }
    }
  } catch (const DepthExceeded& e) {
    r.node = nullptr;
    r.diagnostics.push_back({e.pos, kDepthMessage});
  }
  sort_diagnostics(r.diagnostics);
  return r;
}

}  // namespace

ParseResult parse(std::string_view source) {
  LexResult lexed = lex(source);
  ParseResult r;
  ParserImpl p(std::move(lexed.tokens));
  std::vector<Diagnostic> syntax;
  try {
    r.module = p.module(syntax);
  } catch (const DepthExceeded& e) {
    syntax.push_back({e.pos, kDepthMessage});
  }
  r.diagnostics = std::move(lexed.diagnostics);
  r.diagnostics.insert(r.diagnostics.end(), syntax.begin(), syntax.end());
  sort_diagnostics(r.diagnostics);
  return r;
}

FragmentResult<ExprPtr> parse_expression(std::string_view source) {
  return parse_fragment<ExprPtr>(source, [](ParserImpl& p) { return p.expr(); });
}

FragmentResult<CondPtr> parse_condition(std::string_view source) {
  return parse_fragment<CondPtr>(source, [](ParserImpl& p) { return p.condition(); });
}

FragmentResult<CondPtr> parse_predicate(std::string_view source) {
  return parse_fragment<CondPtr>(source, [](ParserImpl& p) { return p.predicate(); });
}

}  // namespace funny
