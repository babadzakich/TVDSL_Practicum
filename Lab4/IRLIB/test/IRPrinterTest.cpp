#include <gtest/gtest.h>

#include <fstream>
#include <sstream>

#include <string>
#include <string_view>

#include "IRPrinter.h"
#include "Lowering.h"
#include "Sema.h"

namespace {

std::string read_file(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream out;
  out << input.rdbuf();
  return out.str();
}

funnyir::IrModule module_from(std::string_view source) {
  funny::AnalyzeResult analyzed = funny::analyze(source);
  EXPECT_TRUE(analyzed.ok());
  if (!analyzed.ok()) return {};
  return funnyir::lower(analyzed.module);
}

}  // namespace

TEST(IRPrinterTest, prints_stable_text_ir) {
  const auto module = module_from("f(x: int) returns r: int { r = x + 1; }");
  ASSERT_EQ(module.functions.size(), 1U);
  const std::string first = funnyir::print_ir(module);
  const std::string second = funnyir::print_ir(module);

  EXPECT_EQ(first, second);
  EXPECT_NE(first.find("function f"), std::string::npos);
  EXPECT_NE(first.find("%x: int [parameter]"), std::string::npos);
  EXPECT_NE(first.find("%t3: int [temporary]"), std::string::npos);
  EXPECT_NE(first.find("add"), std::string::npos);
  EXPECT_NE(first.find("return"), std::string::npos);
}

TEST(IRPrinterTest, prints_versioned_json_ir) {
  const auto module = module_from(
      "positive(x: int) => x > 0 "
      "f(x: int) requires positive(x) returns r: int ensures r >= 0 { r = x; }");
  ASSERT_EQ(module.functions.size(), 1U);
  const std::string json = funnyir::print_ir_json(module);

  EXPECT_NE(json.find("\"schema_version\":1"), std::string::npos);
  EXPECT_NE(json.find("\"functions\""), std::string::npos);
  EXPECT_NE(json.find("\"requires\""), std::string::npos);
  EXPECT_NE(json.find("\"formulas\""), std::string::npos);
  EXPECT_NE(json.find("positive"), std::string::npos);
}

TEST(IRPrinterTest, prints_cfg_dot_edges) {
  const auto module = module_from(
      "f(x: int) returns r: int { if (x > 0) { r = 1; } else { r = 2; } }");
  ASSERT_EQ(module.functions.size(), 1U);
  const std::string dot = funnyir::print_cfg_dot(module.functions.front());

  EXPECT_NE(dot.find("digraph \"f\""), std::string::npos);
  EXPECT_NE(dot.find("b0 -> b1"), std::string::npos);
  EXPECT_NE(dot.find("b0 -> b2"), std::string::npos);
}

TEST(IRPrinterTest, matches_checked_in_samples) {
  const std::string root = FUNNY_HW4_TESTS_DIR;
  const auto sum = module_from(read_file(root + "/positive/01_sum.funny"));
  ASSERT_EQ(sum.functions.size(), 1U);
  EXPECT_EQ(funnyir::print_ir(sum), read_file(root + "/positive/01_sum.ir"));
  EXPECT_EQ(funnyir::print_ir_json(sum), read_file(root + "/positive/01_sum.json"));

  const auto branch = module_from(read_file(root + "/positive/02_branch_loop.funny"));
  ASSERT_EQ(branch.functions.size(), 1U);
  EXPECT_EQ(funnyir::print_cfg_dot(branch.functions.front()), read_file(root + "/positive/02_branch_loop.cfg.dot"));
}
