#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string_view>

#include "Lowering.h"
#include "RegisterAllocation.h"
#include "Sema.h"

namespace {

funnyir::IrModule lower_source(std::string_view source) {
  funny::AnalyzeResult analyzed = funny::analyze(source);
  EXPECT_TRUE(analyzed.ok());
  if (!analyzed.ok()) return {};
  return funnyir::lower(analyzed.module);
}

funnyir::ValueId value_named(const funnyir::IrFunction& function, std::string_view name) {
  const auto it = std::find_if(function.values.begin(), function.values.end(),
                               [name](const funnyir::IrValue& value) { return value.name == name; });
  return it == function.values.end() ? function.values.size() : it->id;
}

}  // namespace

TEST(LivenessTest, computes_live_in_out_across_branch) {
  const auto module = lower_source(
      "f(x: int) returns r: int uses y: int { if (x > 0) { y = x + 1; } else { y = x - 1; } r = y + 2; }");
  ASSERT_EQ(module.functions.size(), 1U);
  const auto& function = module.functions.front();
  const auto live = funnyir::analyze_liveness(function);
  const auto x = value_named(function, "x");
  const auto y = value_named(function, "y");
  ASSERT_LT(x, function.values.size());
  ASSERT_LT(y, function.values.size());
  EXPECT_TRUE(live.live_in[function.entry].contains(x));

  const auto& branch = std::get<funnyir::BranchTerm>(function.blocks[function.entry].terminator);
  EXPECT_TRUE(live.live_out[branch.true_target].contains(y));
  EXPECT_TRUE(live.live_out[branch.false_target].contains(y));
}

TEST(LivenessTest, converges_on_loop_backedge) {
  const auto module = lower_source("f(n: int) returns r: int uses i: int { i = 0; r = 0; while (i < n) { r = r + i; i = i + 1; } }");
  ASSERT_EQ(module.functions.size(), 1U);
  const auto& function = module.functions.front();
  const auto live = funnyir::analyze_liveness(function);
  const auto i = value_named(function, "i");
  ASSERT_LT(i, function.values.size());
  const auto header = std::find_if(function.blocks.begin(), function.blocks.end(), [](const auto& block) {
    return std::holds_alternative<funnyir::BranchTerm>(block.terminator);
  });
  ASSERT_NE(header, function.blocks.end());
  EXPECT_TRUE(live.live_in[header->id].contains(i));
  const auto body = std::find_if(function.blocks.begin(), function.blocks.end(), [&header](const auto& block) {
    return std::holds_alternative<funnyir::JumpTerm>(block.terminator) &&
           std::get<funnyir::JumpTerm>(block.terminator).target == header->id && !block.instructions.empty();
  });
  ASSERT_NE(body, function.blocks.end());
  EXPECT_TRUE(live.live_out[body->id].contains(i));
}

TEST(LivenessTest, adds_edges_for_simultaneously_live_values) {
  const auto module = lower_source("f(x: int, y: int) returns r: int { r = x + y; }");
  ASSERT_EQ(module.functions.size(), 1U);
  const auto& function = module.functions.front();
  const auto live = funnyir::analyze_liveness(function);
  const auto graph = funnyir::build_interference_graph(function, live);
  const auto x = value_named(function, "x");
  const auto y = value_named(function, "y");

  ASSERT_LT(x, graph.neighbors.size());
  ASSERT_LT(y, graph.neighbors.size());
  EXPECT_TRUE(graph.neighbors[x].contains(y));
  EXPECT_TRUE(graph.neighbors[y].contains(x));
}

TEST(LivenessTest, greedy_colors_by_degree_deterministically) {
  funnyir::InterferenceGraph graph;
  graph.neighbors = {{1, 2}, {0, 2}, {0, 1}, {}};
  const auto first = funnyir::greedy_color(graph);
  const auto second = funnyir::greedy_color(graph);

  EXPECT_EQ(first.colors, second.colors);
  EXPECT_EQ(first.color_count, 3U);
  EXPECT_EQ(first.colors[0], 0U);
  EXPECT_EQ(first.colors[1], 1U);
  EXPECT_EQ(first.colors[2], 2U);
  EXPECT_EQ(first.colors[3], 0U);
}

TEST(LivenessTest, prints_colored_interference_dot) {
  const auto module = lower_source("f(x: int, y: int) returns r: int { r = x + y; }");
  ASSERT_EQ(module.functions.size(), 1U);
  const auto& function = module.functions.front();
  const auto live = funnyir::analyze_liveness(function);
  const auto graph = funnyir::build_interference_graph(function, live);
  const auto colors = funnyir::greedy_color(graph);
  const auto dot = funnyir::print_interference_dot(function, graph, colors);

  EXPECT_NE(dot.find("graph \"f\""), std::string::npos);
  EXPECT_NE(dot.find("xlabel=\"c0\""), std::string::npos);
  EXPECT_NE(dot.find("--"), std::string::npos);
}
