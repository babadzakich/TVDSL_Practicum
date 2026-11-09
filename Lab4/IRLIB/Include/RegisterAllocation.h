#ifndef FUNNYIR_REGISTER_ALLOCATION_H
#define FUNNYIR_REGISTER_ALLOCATION_H

#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include "IR.h"

namespace funnyir {

struct LivenessResult {
  std::vector<std::set<ValueId>> live_in;
  std::vector<std::set<ValueId>> live_out;
};
struct InterferenceGraph {
  std::vector<std::set<ValueId>> neighbors;
};
struct Coloring {
  std::vector<std::size_t> colors;
  std::size_t color_count = 0;
};

LivenessResult analyze_liveness(const IrFunction& function);
InterferenceGraph build_interference_graph(const IrFunction& function, const LivenessResult& liveness);
Coloring greedy_color(const InterferenceGraph& graph);
std::string print_interference_dot(const IrFunction& function, const InterferenceGraph& graph, const Coloring& coloring);

}  // namespace funnyir

#endif  // FUNNYIR_REGISTER_ALLOCATION_H
