// Позиции в исходном тексте и сообщения об ошибках.

#ifndef FUNNYPARSE_DIAGNOSTIC_H
#define FUNNYPARSE_DIAGNOSTIC_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace funny {

// Строка и столбец с 1; столбец — номер байта в строке (алфавит ASCII), таб = 1.
struct Pos {
  int line = 1;
  int col = 1;
  std::size_t offset = 0;
};

struct Diagnostic {
  Pos pos;
  std::string message;
};

// "label:L:C: ошибка: message"
std::string format_diagnostic(std::string_view label, const Diagnostic& d);

// Устойчивая сортировка по смещению.
void sort_diagnostics(std::vector<Diagnostic>& ds);

}  // namespace funny

#endif  // FUNNYPARSE_DIAGNOSTIC_H
