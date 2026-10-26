#ifndef FUNNYPARSE_SNAPSHOT_H
#define FUNNYPARSE_SNAPSHOT_H

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "Diagnostic.h"

namespace funny {

std::string render_diagnostics(std::string_view label, const std::vector<Diagnostic>& ds);

struct SnapshotResult {
  std::filesystem::path source;
  std::string kind;  // "ast" | "json" | "err"
  bool passed = false;
  std::string expected;
  std::string actual;
};

std::vector<SnapshotResult> run_snapshots(const std::filesystem::path& tests_dir, bool bless);

}  // namespace funny

#endif  // FUNNYPARSE_SNAPSHOT_H
