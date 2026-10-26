#ifndef FUNNYSEMA_SEMA_H
#define FUNNYSEMA_SEMA_H

#include <string_view>
#include <vector>

#include "Ast.h"
#include "Diagnostic.h"

namespace funny {

std::vector<Diagnostic> check(Module& m);

struct AnalyzeResult {
  Module module;
  std::vector<Diagnostic> diagnostics;
  bool parsed = false;
  bool ok() const { return diagnostics.empty(); }
};

AnalyzeResult analyze(std::string_view source);

}  // namespace funny

#endif  // FUNNYSEMA_SEMA_H
