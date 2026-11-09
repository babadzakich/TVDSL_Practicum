#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "Cli.h"

namespace {

struct TempDirectory {
  std::filesystem::path path;
  TempDirectory() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    path = std::filesystem::temp_directory_path() / ("funnyir-cli-" + std::to_string(stamp));
    std::filesystem::create_directories(path);
  }
  ~TempDirectory() { std::filesystem::remove_all(path); }
};

const std::string kValidSource = "f(x: int) returns r: int { r = x + 0; }";

}  // namespace

TEST(CliTest, cli_reads_stdin) {
  std::istringstream input("f(x: int) returns r: int { r = x; }");
  std::ostringstream output;
  std::ostringstream error;

  EXPECT_EQ(funnyir::run_cli({}, input, output, error), 0);
  EXPECT_TRUE(error.str().empty());
}

TEST(CliTest, cli_reports_semantic_error_without_ir) {
  std::istringstream input("f(x: int) returns r: int { r = missing; }");
  std::ostringstream output;
  std::ostringstream error;

  EXPECT_EQ(funnyir::run_cli({}, input, output, error), 1);
  EXPECT_TRUE(output.str().empty());
  EXPECT_NE(error.str().find("неизвестное имя 'missing'"), std::string::npos);
}

TEST(CliTest, cli_rejects_missing_file) {
  std::istringstream input;
  std::ostringstream output;
  std::ostringstream error;
  const std::vector<std::string> args{"/tmp/funnyir-file-that-does-not-exist.funny"};

  EXPECT_EQ(funnyir::run_cli(args, input, output, error), 2);
  EXPECT_TRUE(output.str().empty());
  EXPECT_NE(error.str().find("не удалось открыть"), std::string::npos);
}

TEST(CliTest, prints_before_and_after_ir) {
  std::istringstream input(kValidSource);
  std::ostringstream output;
  std::ostringstream error;

  EXPECT_EQ(funnyir::run_cli({}, input, output, error), 0);
  EXPECT_NE(output.str().find("IR before optimizations"), std::string::npos);
  EXPECT_NE(output.str().find("IR after optimizations"), std::string::npos);
  EXPECT_NE(output.str().find("instructions_before"), std::string::npos);
  EXPECT_NE(output.str().find("instructions_after"), std::string::npos);
  EXPECT_TRUE(error.str().empty());
}

TEST(CliTest, disables_all_optimizations) {
  std::istringstream input(kValidSource);
  std::ostringstream output;
  std::ostringstream error;

  EXPECT_EQ(funnyir::run_cli({"--no-opt"}, input, output, error), 0);
  const std::size_t after = output.str().find("IR after optimizations");
  ASSERT_NE(after, std::string::npos);
  EXPECT_NE(output.str().substr(after).find("add"), std::string::npos);
}

TEST(CliTest, disables_one_optimization) {
  std::istringstream input(kValidSource);
  std::ostringstream output;
  std::ostringstream error;

  EXPECT_EQ(funnyir::run_cli({"--no-algebraic"}, input, output, error), 0);
  const std::size_t after = output.str().find("IR after optimizations");
  ASSERT_NE(after, std::string::npos);
  EXPECT_NE(output.str().substr(after).find("add"), std::string::npos);
}

TEST(CliTest, writes_requested_graphs) {
  TempDirectory directory;
  std::istringstream input(kValidSource);
  std::ostringstream output;
  std::ostringstream error;
  const std::string path = directory.path.string();

  EXPECT_EQ(funnyir::run_cli({"--cfg-dot", path, "--interference-dot", path}, input, output, error), 0);
  EXPECT_TRUE(std::filesystem::exists(directory.path / "f.cfg.dot"));
  EXPECT_TRUE(std::filesystem::exists(directory.path / "f.interference.dot"));
  EXPECT_TRUE(error.str().empty());
}

TEST(CliTest, reports_json_metrics) {
  std::istringstream input(kValidSource);
  std::ostringstream output;
  std::ostringstream error;

  EXPECT_EQ(funnyir::run_cli({"--json"}, input, output, error), 0);
  EXPECT_NE(output.str().find("\"metrics\""), std::string::npos);
  EXPECT_NE(output.str().find("\"colors\""), std::string::npos);
  EXPECT_NE(output.str().find("\"temporaries_before\""), std::string::npos);
  EXPECT_NE(output.str().find("\"temporaries_after\""), std::string::npos);
}

TEST(CliTest, rejects_invalid_source_without_partial_ir) {
  std::istringstream input("f(x: int) returns r: int { r = missing; }");
  std::ostringstream output;
  std::ostringstream error;

  EXPECT_EQ(funnyir::run_cli({}, input, output, error), 1);
  EXPECT_TRUE(output.str().empty());
  EXPECT_NE(error.str().find("неизвестное имя 'missing'"), std::string::npos);
}
