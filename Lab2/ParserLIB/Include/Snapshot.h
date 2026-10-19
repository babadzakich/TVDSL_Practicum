// Эталонные тесты: tests/positive/*.funny с .ast (и необязательным .json),
// tests/negative/*.funny с .err.

#ifndef FUNNYPARSE_SNAPSHOT_H
#define FUNNYPARSE_SNAPSHOT_H

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "Diagnostic.h"

namespace funny {

// Каждая диагностика — строка "label:L:C: ошибка: msg\n".
std::string render_diagnostics(std::string_view label, const std::vector<Diagnostic>& ds);

struct SnapshotResult {
  std::filesystem::path source;
  std::string kind;  // "ast" | "json" | "err"
  bool passed = false;
  std::string expected;
  std::string actual;
};

// Метка в диагностиках — имя файла без каталогов. bless: записать фактический
// вывод в эталоны (.ast/.err создаются, .json — только если уже есть).
std::vector<SnapshotResult> run_snapshots(const std::filesystem::path& tests_dir, bool bless);

}  // namespace funny

#endif  // FUNNYPARSE_SNAPSHOT_H
