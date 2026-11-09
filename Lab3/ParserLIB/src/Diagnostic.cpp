#include "Diagnostic.h"

#include <algorithm>

namespace funny {

std::string format_diagnostic(std::string_view label, const Diagnostic& d) {
  std::string out(label);
  out += ':' + std::to_string(d.pos.line) + ':' + std::to_string(d.pos.col) + ": ошибка: ";
  out += d.message;
  return out;
}

void sort_diagnostics(std::vector<Diagnostic>& ds) {
  std::ranges::stable_sort(ds, {}, [](const Diagnostic& d) { return d.pos.offset; });
}

}  // namespace funny
