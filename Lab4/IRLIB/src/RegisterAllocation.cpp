#include "RegisterAllocation.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace funnyir {
namespace {

using ValueSet = std::set<ValueId>;

void add_edge(InterferenceGraph& graph, ValueId lhs, ValueId rhs) {
  if (lhs == rhs || lhs >= graph.neighbors.size() || rhs >= graph.neighbors.size()) return;
  graph.neighbors[lhs].insert(rhs);
  graph.neighbors[rhs].insert(lhs);
}

std::string value_label(const IrFunction& function, ValueId id) {
  if (id >= function.values.size()) return "invalid" + std::to_string(id);
  const IrValue& value = function.values[id];
  const std::string name = value.kind == IrValueKind::Temporary ? "%t" + std::to_string(id) : "%" + value.name;
  return name + ": " + funny::to_string(value.type);
}

std::string palette_color(std::size_t color) {
  static constexpr std::array<const char*, 8> palette = {"#d9eaf7", "#fce5cd", "#d9ead3", "#eadcf8",
                                                         "#f4cccc", "#fff2cc", "#cfe2f3", "#d0e0e3"};
  return palette[color % palette.size()];
}

std::string quote_dot(const std::string& text) {
  std::string out = "\"";
  for (char ch : text) {
    if (ch == '\\' || ch == '"') out += '\\';
    if (ch == '\n') out += "\\n";
    else out += ch;
  }
  return out + '"';
}

}  // namespace

LivenessResult analyze_liveness(const IrFunction& function) {
  const std::size_t count = function.blocks.size();
  LivenessResult result{std::vector<ValueSet>(count), std::vector<ValueSet>(count)};
  std::vector<ValueSet> block_use(count), block_def(count);

  for (const IrBlock& block : function.blocks) {
    ValueSet& uses = block_use[block.id];
    ValueSet& defs = block_def[block.id];
    for (const IrInstruction& instruction : block.instructions) {
      for (ValueId use : instruction.uses) {
        if (!defs.contains(use)) uses.insert(use);
      }
      defs.insert(instruction.defs.begin(), instruction.defs.end());
    }
    if (const auto* branch = std::get_if<BranchTerm>(&block.terminator)) {
      for (ValueId use : branch->uses) {
        if (!defs.contains(use)) uses.insert(use);
      }
    } else if (const auto* ret = std::get_if<ReturnTerm>(&block.terminator)) {
      for (ValueId use : ret->values) {
        if (!defs.contains(use)) uses.insert(use);
      }
    }
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (std::size_t reverse = count; reverse > 0; --reverse) {
      const BlockId id = reverse - 1;
      ValueSet out;
      for (BlockId successor : function.blocks[id].successors) {
        if (successor < count) out.insert(result.live_in[successor].begin(), result.live_in[successor].end());
      }
      ValueSet in = block_use[id];
      for (ValueId value : out) {
        if (!block_def[id].contains(value)) in.insert(value);
      }
      if (out != result.live_out[id] || in != result.live_in[id]) {
        result.live_out[id] = std::move(out);
        result.live_in[id] = std::move(in);
        changed = true;
      }
    }
  }
  return result;
}

InterferenceGraph build_interference_graph(const IrFunction& function, const LivenessResult& liveness) {
  InterferenceGraph graph;
  graph.neighbors.resize(function.values.size());

  if (function.entry < liveness.live_in.size()) {
    const ValueSet& entry_live = liveness.live_in[function.entry];
    for (auto lhs = entry_live.begin(); lhs != entry_live.end(); ++lhs) {
      for (auto rhs = std::next(lhs); rhs != entry_live.end(); ++rhs) add_edge(graph, *lhs, *rhs);
    }
  }

  for (const IrBlock& block : function.blocks) {
    ValueSet live;
    if (block.id < liveness.live_out.size()) live = liveness.live_out[block.id];
    if (const auto* branch = std::get_if<BranchTerm>(&block.terminator)) {
      live.insert(branch->uses.begin(), branch->uses.end());
    } else if (const auto* ret = std::get_if<ReturnTerm>(&block.terminator)) {
      live.insert(ret->values.begin(), ret->values.end());
    }

    for (auto instruction = block.instructions.rbegin(); instruction != block.instructions.rend(); ++instruction) {
      for (ValueId def : instruction->defs) {
        for (ValueId value : live) add_edge(graph, def, value);
      }
      for (auto lhs = instruction->defs.begin(); lhs != instruction->defs.end(); ++lhs) {
        for (auto rhs = std::next(lhs); rhs != instruction->defs.end(); ++rhs) add_edge(graph, *lhs, *rhs);
      }
      for (ValueId def : instruction->defs) live.erase(def);
      live.insert(instruction->uses.begin(), instruction->uses.end());
    }
  }
  return graph;
}

Coloring greedy_color(const InterferenceGraph& graph) {
  const std::size_t count = graph.neighbors.size();
  std::vector<ValueId> order(count);
  for (ValueId id = 0; id < count; ++id) order[id] = id;
  std::sort(order.begin(), order.end(), [&graph](ValueId lhs, ValueId rhs) {
    if (graph.neighbors[lhs].size() != graph.neighbors[rhs].size()) {
      return graph.neighbors[lhs].size() > graph.neighbors[rhs].size();
    }
    return lhs < rhs;
  });

  Coloring result;
  result.colors.resize(count, 0);
  std::vector<bool> assigned(count, false);
  for (ValueId value : order) {
    std::vector<bool> unavailable(result.color_count + 1, false);
    for (ValueId neighbor : graph.neighbors[value]) {
      if (neighbor < count && assigned[neighbor] && result.colors[neighbor] < unavailable.size()) {
        unavailable[result.colors[neighbor]] = true;
      }
    }
    std::size_t color = 0;
    while (color < unavailable.size() && unavailable[color]) ++color;
    result.colors[value] = color;
    assigned[value] = true;
    result.color_count = std::max(result.color_count, color + 1);
  }
  return result;
}

std::string print_interference_dot(const IrFunction& function, const InterferenceGraph& graph, const Coloring& coloring) {
  std::ostringstream out;
  out << "graph " << quote_dot(function.name) << " {\n  node [style=filled];\n";
  for (ValueId id = 0; id < graph.neighbors.size(); ++id) {
    const std::size_t color = id < coloring.colors.size() ? coloring.colors[id] : 0;
    out << "  v" << id << " [label=" << quote_dot(value_label(function, id) + " [color " + std::to_string(color) + "]")
        << ", fillcolor=" << quote_dot(palette_color(color)) << ", xlabel=" << quote_dot("c" + std::to_string(color))
        << "];\n";
  }
  for (ValueId lhs = 0; lhs < graph.neighbors.size(); ++lhs) {
    for (ValueId rhs : graph.neighbors[lhs]) {
      if (lhs < rhs) out << "  v" << lhs << " -- v" << rhs << ";\n";
    }
  }
  out << "}\n";
  return out.str();
}

}  // namespace funnyir
