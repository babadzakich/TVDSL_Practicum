#include "Snapshot.h"

#include <algorithm>
#include <fstream>
#include <optional>
#include <sstream>

#include "AstPrinter.h"
#include "Parser.h"

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

void write_file(const fs::path& path, const std::string& content) {
  std::ofstream out(path, std::ios::binary);
  out << content;
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

// bless_allowed == false: эталон не перезаписывается (позитивный тест с ошибками разбора).
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
    write_file(expected_path, r.actual);
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

std::string render_diagnostics(std::string_view label, const std::vector<Diagnostic>& ds) {
  std::string out;
  for (const Diagnostic& d : ds) {
    out += format_diagnostic(label, d) + '\n';
  }
  return out;
}

std::vector<SnapshotResult> run_snapshots(const fs::path& tests_dir, bool bless) {
  std::vector<SnapshotResult> results;
  for (const fs::path& src : sources_in(tests_dir / "positive")) {
    const std::string text = read_file(src).value_or("");
    const ParseResult r = parse(text);
    const std::string label = src.filename().string();
    results.push_back(
        compare(src, "ast", r.ok() ? print_tree(r.module) : render_diagnostics(label, r.diagnostics), bless, r.ok()));
    fs::path json = src;
    json.replace_extension("json");
    if (fs::exists(json)) {
      results.push_back(
          compare(src, "json", r.ok() ? print_json(r.module) : render_diagnostics(label, r.diagnostics), bless, r.ok()));
    }
  }
  for (const fs::path& src : sources_in(tests_dir / "negative")) {
    const std::string text = read_file(src).value_or("");
    const ParseResult r = parse(text);
    const std::string label = src.filename().string();
    results.push_back(compare(src, "err", r.ok() ? "(нет ошибок)\n" : render_diagnostics(label, r.diagnostics), bless));
  }
  return results;
}

}  // namespace funny
