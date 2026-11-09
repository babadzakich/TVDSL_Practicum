#include <gtest/gtest.h>

#include <algorithm>
#include <string_view>
#include <variant>

#include "Lowering.h"
#include "Sema.h"

namespace {

funnyir::IrModule lower_source(std::string_view source) {
  funny::AnalyzeResult analyzed = funny::analyze(source);
  EXPECT_TRUE(analyzed.ok());
  if (!analyzed.ok()) {
    return {};
  }
  return funnyir::lower(analyzed.module);
}

std::size_t count_opcode(const funnyir::IrFunction& function, funnyir::IrOpcode opcode) {
  std::size_t count = 0;
  for (const funnyir::IrBlock& block : function.blocks) {
    count += static_cast<std::size_t>(std::count_if(block.instructions.begin(), block.instructions.end(),
                                                    [opcode](const funnyir::IrInstruction& instruction) {
                                                      return instruction.opcode == opcode;
                                                    }));
  }
  return count;
}

}  // namespace

TEST(LoweringTest, lowers_arithmetic_to_temporaries) {
  const auto module = lower_source("f(x: int) returns r: int uses t: int { t = x + 1; r = t * 2; }");
  ASSERT_EQ(module.functions.size(), 1U);
  const auto& function = module.functions.front();

  EXPECT_EQ(count_opcode(function, funnyir::IrOpcode::Add), 1U);
  EXPECT_EQ(count_opcode(function, funnyir::IrOpcode::Mul), 1U);
  EXPECT_GE(function.values.size(), 5U);
  EXPECT_TRUE(std::holds_alternative<funnyir::ReturnTerm>(function.blocks[function.exit].terminator));
}

TEST(LoweringTest, lowers_if_and_while_to_cfg) {
  const auto module = lower_source(
      "f(x: int) returns r: int { if (x > 0) { r = 1; } else { r = 2; } while (r < 4) { r = r + 1; } }");
  ASSERT_EQ(module.functions.size(), 1U);
  const auto& function = module.functions.front();
  const auto branches = std::count_if(function.blocks.begin(), function.blocks.end(), [](const funnyir::IrBlock& block) {
    return std::holds_alternative<funnyir::BranchTerm>(block.terminator);
  });
  EXPECT_EQ(branches, 2);
  EXPECT_GE(function.blocks.size(), 7U);
  for (const auto& block : function.blocks) {
    for (const auto successor : block.successors) {
      ASSERT_LT(successor, function.blocks.size());
      EXPECT_NE(std::find(function.blocks[successor].predecessors.begin(), function.blocks[successor].predecessors.end(),
                          block.id),
                function.blocks[successor].predecessors.end());
    }
  }
}

TEST(LoweringTest, lowers_calls_and_tuple_results) {
  const auto module = lower_source(
      "pair(x: int) returns a: int, b: int { a = x; b = x + 1; } "
      "f(x: int) returns a: int, b: int { a, b = pair(x); }");
  ASSERT_EQ(module.functions.size(), 2U);
  const auto& function = module.functions.back();
  ASSERT_EQ(count_opcode(function, funnyir::IrOpcode::Call), 1U);
  for (const auto& block : function.blocks) {
    for (const auto& instruction : block.instructions) {
      if (instruction.opcode == funnyir::IrOpcode::Call) {
        EXPECT_EQ(instruction.defs.size(), 2U);
        EXPECT_EQ(instruction.callee, "pair");
      }
    }
  }
}

TEST(LoweringTest, lowers_array_access_and_update) {
  const auto module = lower_source(
      "f(a: int[], i: int) returns r: int uses t: int[] { t = a; t[i] = 42; r = t[i]; }");
  ASSERT_EQ(module.functions.size(), 1U);
  EXPECT_EQ(count_opcode(module.functions.front(), funnyir::IrOpcode::ArraySet), 1U);
  EXPECT_EQ(count_opcode(module.functions.front(), funnyir::IrOpcode::LoadElement), 1U);
}

TEST(LoweringTest, lowers_predicates_and_contracts) {
  const auto module = lower_source(
      "positive(x: int) => x > 0 "
      "f(x: int) requires positive(x) returns r: int ensures r >= 0 uses t: int { "
      "t = x; while (t > 0) invariant positive(t) { t = t - 1; } r = t; assert r >= 0; assume x >= 0; }");
  ASSERT_EQ(module.formulas.size(), 1U);
  ASSERT_EQ(module.functions.size(), 1U);
  EXPECT_TRUE(module.functions.front().requires_clause.has_value());
  EXPECT_TRUE(module.functions.front().ensures_clause.has_value());
  EXPECT_EQ(count_opcode(module.functions.front(), funnyir::IrOpcode::Assert), 1U);
  EXPECT_EQ(count_opcode(module.functions.front(), funnyir::IrOpcode::Assume), 1U);
  EXPECT_TRUE(std::any_of(module.functions.front().blocks.begin(), module.functions.front().blocks.end(),
                          [](const funnyir::IrBlock& block) { return block.loop_invariant.has_value(); }));
}

TEST(LoweringTest, lowers_condition_calls_before_branch) {
  const auto module = lower_source(
      "inc(x: int) returns r: int { r = x + 1; } "
      "f(x: int) returns r: int { if (inc(x) > 0) { r = 1; } else { r = 0; } }");
  ASSERT_EQ(module.functions.size(), 2U);
  const auto& function = module.functions.back();
  const auto branch = std::find_if(function.blocks.begin(), function.blocks.end(), [](const funnyir::IrBlock& block) {
    return std::holds_alternative<funnyir::BranchTerm>(block.terminator);
  });
  ASSERT_NE(branch, function.blocks.end());
  EXPECT_NE(std::find_if(branch->instructions.begin(), branch->instructions.end(), [](const auto& instruction) {
              return instruction.opcode == funnyir::IrOpcode::Call;
            }),
            branch->instructions.end());
}

TEST(LoweringTest, exits_return_declared_values) {
  const auto module = lower_source("f(x: int) returns r: int { r = x; }");
  ASSERT_EQ(module.functions.size(), 1U);
  const auto& function = module.functions.front();
  ASSERT_TRUE(std::holds_alternative<funnyir::ReturnTerm>(function.blocks[function.exit].terminator));
  EXPECT_EQ(std::get<funnyir::ReturnTerm>(function.blocks[function.exit].terminator).values.size(), 1U);
}
