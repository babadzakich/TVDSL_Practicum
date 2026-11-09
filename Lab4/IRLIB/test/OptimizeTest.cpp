#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string_view>
#include <variant>

#include "IRInterpreter.h"
#include "Lowering.h"
#include "Optimize.h"
#include "Sema.h"

namespace {

funnyir::IrModule lower_source(std::string_view source) {
  funny::AnalyzeResult analyzed = funny::analyze(source);
  EXPECT_TRUE(analyzed.ok());
  if (!analyzed.ok()) return {};
  return funnyir::lower(analyzed.module);
}

funnyir::IrModule optimize_source(std::string_view source, funnyir::OptimizationOptions options) {
  auto module = lower_source(source);
  funnyir::optimize(module, options);
  return module;
}

std::vector<const funnyir::IrInstruction*> instructions(const funnyir::IrFunction& function,
                                                         funnyir::IrOpcode opcode) {
  std::vector<const funnyir::IrInstruction*> found;
  for (const auto& block : function.blocks) {
    for (const auto& instruction : block.instructions) {
      if (instruction.opcode == opcode) found.push_back(&instruction);
    }
  }
  return found;
}

}  // namespace

TEST(OptimizeTest, propagates_local_constants) {
  auto module = optimize_source("f() returns r: int uses x: int, y: int { x = 2; y = 3; r = x + y; }",
                                {.constant_propagation = true, .algebraic_simplification = false,
                                 .dead_code_elimination = false});
  ASSERT_EQ(module.functions.size(), 1U);
  const auto adds = instructions(module.functions.front(), funnyir::IrOpcode::Add);
  ASSERT_EQ(adds.size(), 1U);
  ASSERT_EQ(adds.front()->operands.size(), 2U);
  const auto* left = std::get_if<std::int64_t>(&adds.front()->operands[0]);
  const auto* right = std::get_if<std::int64_t>(&adds.front()->operands[1]);
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(*left, 2);
  EXPECT_EQ(*right, 3);
}

TEST(OptimizeTest, invalidates_redefined_slots) {
  auto module = optimize_source("f() returns r: int uses x: int { x = 1; r = x + 0; x = 2; r = r + x; }",
                                {.constant_propagation = true, .algebraic_simplification = false,
                                 .dead_code_elimination = false});
  ASSERT_EQ(module.functions.size(), 1U);
  const auto adds = instructions(module.functions.front(), funnyir::IrOpcode::Add);
  ASSERT_EQ(adds.size(), 2U);
  ASSERT_EQ(adds.back()->operands.size(), 2U);
  const auto* right = std::get_if<std::int64_t>(&adds.back()->operands[1]);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(*right, 2);
}

TEST(OptimizeTest, simplifies_safe_identities) {
  auto module = optimize_source("f(x: int) returns r: int { r = x * 1; }",
                                {.constant_propagation = false, .algebraic_simplification = true,
                                 .dead_code_elimination = false});
  ASSERT_EQ(module.functions.size(), 1U);
  EXPECT_TRUE(instructions(module.functions.front(), funnyir::IrOpcode::Mul).empty());
  EXPECT_EQ(instructions(module.functions.front(), funnyir::IrOpcode::Copy).size(), 2U);
}

TEST(OptimizeTest, removes_dead_safe_ops) {
  auto module = optimize_source("f(x: int) returns r: int uses t: int { t = x + 2; r = x; }",
                                {.constant_propagation = false, .algebraic_simplification = false,
                                 .dead_code_elimination = true});
  ASSERT_EQ(module.functions.size(), 1U);
  EXPECT_TRUE(instructions(module.functions.front(), funnyir::IrOpcode::Add).empty());
  EXPECT_EQ(instructions(module.functions.front(), funnyir::IrOpcode::Copy).size(), 1U);
}

TEST(OptimizeTest, preserves_dead_trapping_division_and_calls) {
  auto module = optimize_source(
      "trap(x: int) returns z: int { z = 1 / x; } "
      "f(x: int) returns r: int uses t: int { t = trap(x); r = x; t = 1 / x; }",
      {.constant_propagation = true, .algebraic_simplification = true, .dead_code_elimination = true});
  ASSERT_EQ(module.functions.size(), 2U);
  EXPECT_EQ(instructions(module.functions.back(), funnyir::IrOpcode::Call).size(), 1U);
  EXPECT_EQ(instructions(module.functions.back(), funnyir::IrOpcode::Div).size(), 1U);
}

TEST(OptimizeTest, optimizations_preserve_execution) {
  auto original = lower_source(
      "calc(x: int) returns r: int uses t: int { t = x + 0; if (t > 0) { r = t * 2 + 3; } else { r = 0; } }");
  ASSERT_EQ(original.functions.size(), 1U);
  auto optimized = original;
  funnyir::optimize(optimized, {});

  for (std::int32_t arg : {5, -5}) {
    const auto before = funnyir_test::execute(original, "calc", {arg});
    const auto after = funnyir_test::execute(optimized, "calc", {arg});
    ASSERT_FALSE(before.trapped);
    ASSERT_FALSE(after.trapped);
    ASSERT_EQ(before.results.size(), 1U);
    ASSERT_EQ(after.results.size(), 1U);
    EXPECT_EQ(std::get<std::int32_t>(before.results.front()), std::get<std::int32_t>(after.results.front()));
  }
}

TEST(OptimizeTest, preserves_array_calls_and_loop_execution) {
  auto original = lower_source(
      "inc(x: int) returns r: int { r = x + 1; } "
      "update(a: int[], n: int) returns r: int[] uses i: int, v: int { "
      "r = a; i = 0; while (i < n) { v = inc(r[i]); r[i] = v; i = i + 1; } }");
  ASSERT_EQ(original.functions.size(), 2U);
  auto optimized = original;
  funnyir::optimize(optimized, {});

  const std::vector<funnyir_test::Value> args{std::vector<std::int32_t>{0, 4, -1}, std::int32_t{3}};
  const auto before = funnyir_test::execute(original, "update", args);
  const auto after = funnyir_test::execute(optimized, "update", args);
  ASSERT_FALSE(before.trapped) << before.error;
  ASSERT_FALSE(after.trapped) << after.error;
  ASSERT_EQ(before.results.size(), 1U);
  ASSERT_EQ(after.results.size(), 1U);
  EXPECT_EQ(std::get<std::vector<std::int32_t>>(before.results.front()),
            std::get<std::vector<std::int32_t>>(after.results.front()));
}

TEST(OptimizeTest, preserves_division_traps) {
  auto original = lower_source("divide(x: int) returns r: int { r = 10 / x; }");
  auto optimized = original;
  funnyir::optimize(optimized, {});

  const auto before = funnyir_test::execute(original, "divide", {std::int32_t{0}});
  const auto after = funnyir_test::execute(optimized, "divide", {std::int32_t{0}});
  EXPECT_TRUE(before.trapped);
  EXPECT_TRUE(after.trapped);
  EXPECT_EQ(before.error, after.error);
}

TEST(OptimizeTest, folds_int_overflow_modulo_32_bits) {
  auto original = lower_source("wrap() returns r: int { r = 2147483647 + 1; }");
  auto optimized = original;
  funnyir::optimize(optimized, {});
  ASSERT_EQ(optimized.functions.size(), 1U);
  EXPECT_TRUE(instructions(optimized.functions.front(), funnyir::IrOpcode::Add).empty());
  const auto result = funnyir_test::execute(optimized, "wrap", {});
  ASSERT_FALSE(result.trapped);
  ASSERT_EQ(result.results.size(), 1U);
  EXPECT_EQ(std::get<std::int32_t>(result.results.front()), std::numeric_limits<std::int32_t>::min());
}

TEST(OptimizeTest, keeps_constant_division_by_zero) {
  auto module = optimize_source("f() returns r: int uses t: int { t = 1 / 0; r = 1; }", {});
  ASSERT_EQ(module.functions.size(), 1U);
  EXPECT_EQ(instructions(module.functions.front(), funnyir::IrOpcode::Div).size(), 1U);
}
