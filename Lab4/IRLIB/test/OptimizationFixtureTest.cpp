#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "IRInterpreter.h"
#include "IRPrinter.h"
#include "Lowering.h"
#include "Optimize.h"
#include "Sema.h"

namespace {

std::string read_file(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return {};
  std::ostringstream content;
  content << input.rdbuf();
  return content.str();
}

struct OptimizationFixture {
  const char* name;
  const char* function;
  std::vector<std::int32_t> arguments;
  const char* expected_after;
  std::int32_t expected_result;
};

void check_fixture(const OptimizationFixture& fixture) {
  const std::string root = FUNNY_HW4_TESTS_DIR;
  const std::string prefix = root + "/positive/" + fixture.name;
  const std::string source = read_file(prefix + ".funny");
  ASSERT_FALSE(source.empty()) << "missing or empty optimization source: " << prefix << ".funny";

  const funny::AnalyzeResult analyzed = funny::analyze(source);
  ASSERT_TRUE(analyzed.ok()) << "invalid optimization source: " << prefix << ".funny";
  const funnyir::IrModule before = funnyir::lower(analyzed.module);
  ASSERT_FALSE(before.functions.empty());
  const std::string before_snapshot = read_file(prefix + ".before.ir");
  ASSERT_FALSE(before_snapshot.empty()) << "missing IR snapshot: " << prefix << ".before.ir";
  EXPECT_EQ(funnyir::print_ir(before), before_snapshot);

  funnyir::IrModule after = before;
  funnyir::optimize(after);
  const std::string after_snapshot = read_file(prefix + ".after.ir");
  ASSERT_FALSE(after_snapshot.empty()) << "missing IR snapshot: " << prefix << ".after.ir";
  const std::string optimized_ir = funnyir::print_ir(after);
  EXPECT_EQ(optimized_ir, after_snapshot);
  EXPECT_NE(optimized_ir.find(fixture.expected_after), std::string::npos) << fixture.name;

  std::vector<funnyir_test::Value> arguments;
  for (std::int32_t argument : fixture.arguments) arguments.emplace_back(argument);
  const auto before_result = funnyir_test::execute(before, fixture.function, arguments);
  const auto after_result = funnyir_test::execute(after, fixture.function, arguments);
  ASSERT_FALSE(before_result.trapped) << fixture.name << ": " << before_result.error;
  ASSERT_FALSE(after_result.trapped) << fixture.name << ": " << after_result.error;
  ASSERT_EQ(before_result.trapped, after_result.trapped) << fixture.name;
  ASSERT_EQ(before_result.error, after_result.error) << fixture.name;
  ASSERT_EQ(before_result.results.size(), 1U) << fixture.name;
  ASSERT_EQ(after_result.results.size(), 1U) << fixture.name;
  for (std::size_t i = 0; i < before_result.results.size(); ++i) {
    EXPECT_EQ(before_result.results[i], after_result.results[i]) << fixture.name << " result " << i;
  }
  ASSERT_TRUE(std::holds_alternative<std::int32_t>(after_result.results.front())) << fixture.name;
  EXPECT_EQ(std::get<std::int32_t>(after_result.results.front()), fixture.expected_result) << fixture.name;
}

}  // namespace

TEST(OptimizationFixtureTest, constant_propagation_fixture_matches_and_preserves_result) {
  check_fixture({"04_constprop", "constant_result", {}, "%r = copy 42", 42});
}

TEST(OptimizationFixtureTest, algebraic_simplification_fixture_matches_and_preserves_results) {
  for (std::int32_t input : {-17, 0, 42}) {
    SCOPED_TRACE(input);
    check_fixture({"05_algebraic", "simplified", {input}, "%t5 = copy %x", input});
  }
}

TEST(OptimizationFixtureTest, dead_code_elimination_fixture_matches_and_preserves_result) {
  for (std::int32_t input : {-9, 0, 13}) {
    SCOPED_TRACE(input);
    check_fixture({"06_dce", "live_result", {input}, "%r = copy %x", input});
  }
}
