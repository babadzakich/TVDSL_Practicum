#include "SemaSnapshot.h"

#include <algorithm>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include "AstPrinter.h"
#include "Sema.h"
#include "Symbols.h"

namespace funny {

namespace {

namespace fs = std::filesystem;

std::optional<std::string> read_file(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return std::nullopt;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

std::vector<fs::path> sources_in(const fs::path& dir) {
  std::vector<fs::path> out;
  if (fs::is_directory(dir)) {
    for (const auto& entry : fs::directory_iterator(dir)) {
      if (entry.path().extension() == ".funny") {
        out.push_back(entry.path());
      }
    }
  }
  std::ranges::sort(out);
  return out;
}

SnapshotResult compare(const fs::path& source, const std::string& kind, std::string actual, bool bless,
                       bool bless_allowed = true) {
  fs::path expected_path = source;
  expected_path.replace_extension(kind);
  SnapshotResult r{source, kind, false, "", std::move(actual)};
  if (bless && !bless_allowed) {
    r.expected = "(позитивный тест не должен давать ошибок; эталон не записан)\n";
    return r;
  }
  if (bless) {
    std::ofstream(expected_path, std::ios::binary) << r.actual;
    r.expected = r.actual;
    r.passed = true;
    return r;
  }
  const std::optional<std::string> expected = read_file(expected_path);
  r.expected = expected ? *expected : "(нет файла " + expected_path.filename().string() + ")\n";
  r.passed = expected && *expected == r.actual;
  return r;
}

}  // namespace

std::vector<SnapshotResult> run_sema_snapshots(const fs::path& tests_dir, bool bless) {
  std::vector<SnapshotResult> results;
  for (const fs::path& src : sources_in(tests_dir / "positive")) {
    const AnalyzeResult r = analyze(read_file(src).value_or(""));
    const std::string errors = render_diagnostics(src.filename().string(), r.diagnostics);
    results.push_back(compare(src, "ast", r.ok() ? print_tree(r.module) : errors, bless, r.ok()));
    for (const char* kind : {"json", "sym"}) {
      fs::path extra = src;
      extra.replace_extension(kind);
      if (fs::exists(extra)) {
        const std::string out = std::string(kind) == "json" ? print_json(r.module) : print_symbols(r.module);
        results.push_back(compare(src, kind, r.ok() ? out : errors, bless, r.ok()));
      }
    }
  }
  for (const fs::path& src : sources_in(tests_dir / "negative")) {
    const AnalyzeResult r = analyze(read_file(src).value_or(""));
    results.push_back(compare(
        src, "err", r.ok() ? "(нет ошибок)\n" : render_diagnostics(src.filename().string(), r.diagnostics), bless));
  }
  return results;
}

}  // namespace funny
