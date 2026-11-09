#include "IR.h"

namespace funnyir {

std::vector<BlockId> successors(const IrTerminator& terminator) {
  return std::visit(
      [](const auto& node) -> std::vector<BlockId> {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, JumpTerm>) {
          return {node.target};
        } else if constexpr (std::is_same_v<T, BranchTerm>) {
          return {node.true_target, node.false_target};
        } else {
          return {};
        }
      },
      terminator);
}

void rebuild_cfg(IrFunction& function) {
  for (IrBlock& block : function.blocks) {
    block.successors = successors(block.terminator);
    block.predecessors.clear();
  }
  for (const IrBlock& block : function.blocks) {
    for (BlockId target : block.successors) {
      if (target < function.blocks.size()) {
        function.blocks[target].predecessors.push_back(block.id);
      }
    }
  }
}

}  // namespace funnyir
