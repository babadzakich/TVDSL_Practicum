#include "Sema.h"

#include "Checker.h"
#include "Overloaded.h"
#include "Parser.h"
#include "Symbols.h"

namespace funny {

std::vector<Diagnostic> check(Module& m) {
  std::vector<Diagnostic> diags;
  const GlobalScope globals(m, diags);
  Checker checker(globals, diags);
  for (Decl& d : m.decls) {
    std::visit(Overloaded{
                   [&](Function& f) { checker.function(f); },
                   [&](Formula& f) { checker.formula(f); },
               },
               d);
  }
  sort_diagnostics(diags);
  return diags;
}

AnalyzeResult analyze(std::string_view source) {
  ParseResult p = parse(source);
  AnalyzeResult r{std::move(p.module), std::move(p.diagnostics), false};
  if (r.diagnostics.empty()) {
    r.parsed = true;
    r.diagnostics = check(r.module);
  }
  return r;
}

}  // namespace funny
