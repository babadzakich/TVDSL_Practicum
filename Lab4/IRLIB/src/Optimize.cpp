#include "Optimize.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Overloaded.h"

namespace funnyir {
namespace {

std::int32_t as_i32(std::int64_t value) {
  return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

std::int32_t wrap_add(std::int32_t lhs, std::int32_t rhs) {
  return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(lhs) + std::bit_cast<std::uint32_t>(rhs));
}
std::int32_t wrap_sub(std::int32_t lhs, std::int32_t rhs) {
  return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(lhs) - std::bit_cast<std::uint32_t>(rhs));
}
std::int32_t wrap_mul(std::int32_t lhs, std::int32_t rhs) {
  return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(lhs) * std::bit_cast<std::uint32_t>(rhs));
}

std::optional<std::int64_t> immediate(const IrOperand& operand) {
  if (const auto* value = std::get_if<std::int64_t>(&operand)) return *value;
  return std::nullopt;
}

void refresh_uses(IrInstruction& instruction) {
  if (instruction.opcode == IrOpcode::Assert || instruction.opcode == IrOpcode::Assume) return;
  instruction.uses.clear();
  for (const IrOperand& operand : instruction.operands) {
    if (const auto* value = std::get_if<ValueRef>(&operand)) {
      if (std::find(instruction.uses.begin(), instruction.uses.end(), value->id) == instruction.uses.end()) {
        instruction.uses.push_back(value->id);
      }
    }
  }
}

void propagate_block(IrBlock& block) {
  std::unordered_map<ValueId, std::int64_t> constants;
  for (IrInstruction& instruction : block.instructions) {
    if (instruction.opcode != IrOpcode::Constant && instruction.opcode != IrOpcode::Assert &&
        instruction.opcode != IrOpcode::Assume) {
      for (IrOperand& operand : instruction.operands) {
        if (auto* value = std::get_if<ValueRef>(&operand)) {
          if (const auto found = constants.find(value->id); found != constants.end()) operand = found->second;
        }
      }
      refresh_uses(instruction);
    }

    for (ValueId def : instruction.defs) constants.erase(def);
    if (instruction.defs.size() != 1) continue;
    if (instruction.opcode == IrOpcode::Constant && !instruction.operands.empty()) {
      if (const auto value = immediate(instruction.operands.front())) constants[instruction.defs.front()] = *value;
    } else if (instruction.opcode == IrOpcode::Copy && !instruction.operands.empty()) {
      if (const auto value = immediate(instruction.operands.front())) constants[instruction.defs.front()] = *value;
    }
  }
}

std::optional<std::int32_t> constant_at(const IrInstruction& instruction, std::size_t index) {
  if (index >= instruction.operands.size()) return std::nullopt;
  const auto value = immediate(instruction.operands[index]);
  if (!value) return std::nullopt;
  return as_i32(*value);
}

void rewrite_constant(IrInstruction& instruction, std::int32_t value) {
  instruction.opcode = IrOpcode::Constant;
  instruction.operands = {static_cast<std::int64_t>(value)};
  instruction.uses.clear();
  instruction.callee.clear();
  instruction.predicate.reset();
}

void rewrite_copy(IrInstruction& instruction, const IrOperand& source) {
  instruction.opcode = IrOpcode::Copy;
  instruction.operands = {source};
  instruction.callee.clear();
  instruction.predicate.reset();
  refresh_uses(instruction);
}

void simplify_block(IrBlock& block) {
  std::unordered_map<ValueId, std::size_t> definitions;
  for (std::size_t i = 0; i < block.instructions.size(); ++i) {
    for (ValueId def : block.instructions[i].defs) definitions[def] = i;
  }
  std::unordered_map<ValueId, std::int64_t> constants;

  for (std::size_t i = 0; i < block.instructions.size(); ++i) {
    IrInstruction& instruction = block.instructions[i];
    if (instruction.opcode != IrOpcode::Assert && instruction.opcode != IrOpcode::Assume) {
      for (IrOperand& operand : instruction.operands) {
        if (auto* ref = std::get_if<ValueRef>(&operand)) {
          if (const auto found = constants.find(ref->id); found != constants.end()) operand = found->second;
        }
      }
      refresh_uses(instruction);
    }

    if (instruction.opcode == IrOpcode::Neg && instruction.operands.size() == 1) {
      if (const auto value = constant_at(instruction, 0)) {
        rewrite_constant(instruction, std::bit_cast<std::int32_t>(0U - std::bit_cast<std::uint32_t>(*value)));
      } else if (const auto* ref = std::get_if<ValueRef>(&instruction.operands.front())) {
        if (const auto found = definitions.find(ref->id); found != definitions.end() && found->second < i) {
          const IrInstruction& inner = block.instructions[found->second];
          if (inner.opcode == IrOpcode::Neg && inner.operands.size() == 1) rewrite_copy(instruction, inner.operands.front());
        }
      }
    }

    const bool binary = instruction.opcode == IrOpcode::Add || instruction.opcode == IrOpcode::Sub ||
                        instruction.opcode == IrOpcode::Mul || instruction.opcode == IrOpcode::Div;
    if (binary && instruction.operands.size() == 2) {
      const auto lhs_const = constant_at(instruction, 0);
      const auto rhs_const = constant_at(instruction, 1);
      if (lhs_const && rhs_const) {
        std::optional<std::int32_t> folded;
        switch (instruction.opcode) {
          case IrOpcode::Add: folded = wrap_add(*lhs_const, *rhs_const); break;
          case IrOpcode::Sub: folded = wrap_sub(*lhs_const, *rhs_const); break;
          case IrOpcode::Mul: folded = wrap_mul(*lhs_const, *rhs_const); break;
          case IrOpcode::Div:
            if (*rhs_const != 0) {
              if (*lhs_const == std::numeric_limits<std::int32_t>::min() && *rhs_const == -1) {
                folded = *lhs_const;
              } else {
                folded = *lhs_const / *rhs_const;
              }
            }
            break;
          default: break;
        }
        if (folded) rewrite_constant(instruction, *folded);
      } else if (instruction.opcode == IrOpcode::Add) {
        if (lhs_const && *lhs_const == 0) rewrite_copy(instruction, instruction.operands[1]);
        else if (rhs_const && *rhs_const == 0) rewrite_copy(instruction, instruction.operands[0]);
      } else if (instruction.opcode == IrOpcode::Sub) {
        if (rhs_const && *rhs_const == 0) rewrite_copy(instruction, instruction.operands[0]);
      } else if (instruction.opcode == IrOpcode::Mul) {
        if ((lhs_const && *lhs_const == 0) || (rhs_const && *rhs_const == 0)) {
          rewrite_constant(instruction, 0);
        } else if (lhs_const && *lhs_const == 1) {
          rewrite_copy(instruction, instruction.operands[1]);
        } else if (rhs_const && *rhs_const == 1) {
          rewrite_copy(instruction, instruction.operands[0]);
        }
      } else if (instruction.opcode == IrOpcode::Div && rhs_const && *rhs_const == 1) {
        rewrite_copy(instruction, instruction.operands[0]);
      }
    }

    for (ValueId def : instruction.defs) constants.erase(def);
    if (instruction.defs.size() == 1 && !instruction.operands.empty() &&
        (instruction.opcode == IrOpcode::Constant || instruction.opcode == IrOpcode::Copy)) {
      if (const auto value = immediate(instruction.operands.front())) constants[instruction.defs.front()] = *value;
    }
  }
}

void add_predicate_names(const IrPredicateExpr& expr, std::unordered_set<std::string>& names) {
  std::visit(
      funny::Overloaded{
          [](const IrIntLiteral&) {},
          [&names](const IrVarRef& ref) {
            if (ref.kind != funny::VarKind::Bound) names.insert(ref.name);
          },
          [&names](const IrArrayAccess& access) {
            names.insert(access.array);
            add_predicate_names(*access.index, names);
          },
          [&names](const IrCallExpr& call) {
            for (const auto& arg : call.args) add_predicate_names(*arg, names);
          },
          [&names](const IrNegExpr& neg) { add_predicate_names(*neg.operand, names); },
          [&names](const IrBinaryExpr& binary) {
            add_predicate_names(*binary.lhs, names);
            add_predicate_names(*binary.rhs, names);
          },
          [](const IrPredicateValue&) {},
      },
      expr.node);
}

void add_predicate_names(const IrPredicate& predicate, std::unordered_set<std::string>& names) {
  std::visit(
      funny::Overloaded{
          [](const IrBoolLiteral&) {},
          [&names](const IrCompare& compare) {
            add_predicate_names(*compare.lhs, names);
            add_predicate_names(*compare.rhs, names);
          },
          [&names](const IrNot& node) { add_predicate_names(*node.operand, names); },
          [&names](const IrLogical& node) {
            add_predicate_names(*node.lhs, names);
            add_predicate_names(*node.rhs, names);
          },
          [&names](const IrQuantifier& node) { add_predicate_names(*node.body, names); },
          [&names](const IrFormulaRef& node) {
            for (const auto& arg : node.args) add_predicate_names(*arg, names);
          },
      },
      predicate.node);
}

bool removable_if_dead(IrOpcode opcode) {
  return opcode == IrOpcode::Constant || opcode == IrOpcode::Copy || opcode == IrOpcode::Neg ||
         opcode == IrOpcode::Add || opcode == IrOpcode::Sub || opcode == IrOpcode::Mul;
}

void eliminate_dead_code(IrFunction& function) {
  std::unordered_set<std::string> metadata_names;
  if (function.requires_clause) add_predicate_names(*function.requires_clause, metadata_names);
  if (function.ensures_clause) add_predicate_names(*function.ensures_clause, metadata_names);
  for (const IrBlock& block : function.blocks) {
    if (block.loop_invariant) add_predicate_names(*block.loop_invariant, metadata_names);
  }
  std::unordered_set<ValueId> metadata_uses;
  for (const IrValue& value : function.values) {
    if (metadata_names.contains(value.name)) metadata_uses.insert(value.id);
  }

  std::vector<std::unordered_set<ValueId>> uses_by_block(function.blocks.size());
  for (const IrBlock& block : function.blocks) {
    auto& uses = uses_by_block[block.id];
    for (const IrInstruction& instruction : block.instructions) uses.insert(instruction.uses.begin(), instruction.uses.end());
    if (const auto* branch = std::get_if<BranchTerm>(&block.terminator)) {
      uses.insert(branch->uses.begin(), branch->uses.end());
    } else if (const auto* ret = std::get_if<ReturnTerm>(&block.terminator)) {
      uses.insert(ret->values.begin(), ret->values.end());
    }
  }

  for (IrBlock& block : function.blocks) {
    std::unordered_set<ValueId> live = metadata_uses;
    for (std::size_t other = 0; other < uses_by_block.size(); ++other) {
      if (other == block.id) continue;
      live.insert(uses_by_block[other].begin(), uses_by_block[other].end());
    }
    if (const auto* branch = std::get_if<BranchTerm>(&block.terminator)) {
      live.insert(branch->uses.begin(), branch->uses.end());
    } else if (const auto* ret = std::get_if<ReturnTerm>(&block.terminator)) {
      live.insert(ret->values.begin(), ret->values.end());
    }

    std::vector<IrInstruction> kept;
    kept.reserve(block.instructions.size());
    for (auto it = block.instructions.rbegin(); it != block.instructions.rend(); ++it) {
      const IrInstruction& instruction = *it;
      const bool has_live_def = std::any_of(instruction.defs.begin(), instruction.defs.end(),
                                            [&live](ValueId value) { return live.contains(value); });
      if (removable_if_dead(instruction.opcode) && !instruction.defs.empty() && !has_live_def) continue;
      for (ValueId def : instruction.defs) live.erase(def);
      live.insert(instruction.uses.begin(), instruction.uses.end());
      kept.push_back(instruction);
    }
    std::reverse(kept.begin(), kept.end());
    block.instructions = std::move(kept);
  }
}

}  // namespace

void optimize(IrModule& module, const OptimizationOptions& options) {
  for (IrFunction& function : module.functions) {
    if (options.constant_propagation) {
      for (IrBlock& block : function.blocks) propagate_block(block);
    }
    if (options.algebraic_simplification) {
      for (IrBlock& block : function.blocks) simplify_block(block);
    }
    if (options.dead_code_elimination) eliminate_dead_code(function);
  }
}

}  // namespace funnyir
