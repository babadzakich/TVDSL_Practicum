#include "IRInterpreter.h"
#include "Overloaded.h"

#include <bit>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace funnyir_test {
namespace {

std::int32_t wrap_int(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
std::uint32_t int_bits(std::int32_t value) { return std::bit_cast<std::uint32_t>(value); }
std::int32_t from_literal(std::int64_t value) { return wrap_int(static_cast<std::uint32_t>(value)); }

std::int32_t as_int(const Value& value) {
  if (const auto* integer = std::get_if<std::int32_t>(&value)) return *integer;
  throw std::runtime_error("ожидался int в IR-интерпретаторе");
}

std::vector<std::int32_t> as_array(const Value& value) {
  if (const auto* array = std::get_if<std::vector<std::int32_t>>(&value)) return *array;
  throw std::runtime_error("ожидался int[] в IR-интерпретаторе");
}

std::int32_t arithmetic(funny::BinOp op, std::int32_t lhs, std::int32_t rhs) {
  switch (op) {
    case funny::BinOp::Add: return wrap_int(int_bits(lhs) + int_bits(rhs));
    case funny::BinOp::Sub: return wrap_int(int_bits(lhs) - int_bits(rhs));
    case funny::BinOp::Mul: return wrap_int(int_bits(lhs) * int_bits(rhs));
    case funny::BinOp::Div:
      if (rhs == 0) throw std::runtime_error("division by zero");
      if (lhs == std::numeric_limits<std::int32_t>::min() && rhs == -1) return lhs;
      return lhs / rhs;
  }
  throw std::runtime_error("неизвестная арифметическая операция");
}

bool compare(funny::CmpOp op, std::int32_t lhs, std::int32_t rhs) {
  switch (op) {
    case funny::CmpOp::Eq: return lhs == rhs;
    case funny::CmpOp::Ne: return lhs != rhs;
    case funny::CmpOp::Ge: return lhs >= rhs;
    case funny::CmpOp::Le: return lhs <= rhs;
    case funny::CmpOp::Gt: return lhs > rhs;
    case funny::CmpOp::Lt: return lhs < rhs;
  }
  return false;
}

class Interpreter {
 public:
  explicit Interpreter(const funnyir::IrModule& module) : module_(module) {}

  ExecutionResult run(std::string_view name, const std::vector<Value>& args) {
    try {
      return invoke(name, args, 0);
    } catch (const std::exception& error) {
      return {{}, true, error.what()};
    }
  }

 private:
  const funnyir::IrModule& module_;
  std::size_t steps_ = 0;

  const funnyir::IrFunction& find_function(std::string_view name) const {
    for (const auto& function : module_.functions) {
      if (function.name == name) return function;
    }
    throw std::runtime_error("unknown function: " + std::string(name));
  }

  funnyir::ValueId named_value(const funnyir::IrFunction& function, const std::string& name) const {
    for (const auto& value : function.values) {
      if (value.name == name) return value.id;
    }
    throw std::runtime_error("unknown IR variable: " + name);
  }

  Value operand(const funnyir::IrFunction& function, const std::vector<Value>& values,
                const funnyir::IrOperand& value) const {
    return std::visit(
        funny::Overloaded{[&values](funnyir::ValueRef ref) { return values.at(ref.id); },
                          [](std::int64_t integer) -> Value { return from_literal(integer); }},
        value);
  }

  Value predicate_expr(const funnyir::IrFunction& function, const std::vector<Value>& values,
                       const funnyir::IrPredicateExpr& expr) {
    return std::visit(
        funny::Overloaded{
            [](const funnyir::IrIntLiteral& literal) -> Value { return from_literal(std::stoll(literal.text)); },
            [this, &function, &values](const funnyir::IrVarRef& ref) -> Value {
              return values.at(named_value(function, ref.name));
            },
            [this, &function, &values](const funnyir::IrArrayAccess& access) -> Value {
              const auto& array = as_array(values.at(named_value(function, access.array)));
              const std::int32_t index = as_int(predicate_expr(function, values, *access.index));
              if (index < 0 || static_cast<std::size_t>(index) >= array.size()) throw std::runtime_error("array bounds");
              return array[static_cast<std::size_t>(index)];
            },
            [this, &function, &values](const funnyir::IrCallExpr& call) -> Value {
              std::vector<Value> args;
              for (const auto& arg : call.args) args.push_back(predicate_expr(function, values, *arg));
              if (call.callee == "length") {
                if (args.size() != 1) throw std::runtime_error("invalid length arity");
                return static_cast<std::int32_t>(as_array(args.front()).size());
              }
              const auto result = invoke(call.callee, args, 1);
              if (result.trapped || result.results.empty()) throw std::runtime_error(result.error.empty() ? "call failed" : result.error);
              return result.results.front();
            },
            [this, &function, &values](const funnyir::IrNegExpr& neg) -> Value {
              return wrap_int(0U - int_bits(as_int(predicate_expr(function, values, *neg.operand))));
            },
            [this, &function, &values](const funnyir::IrBinaryExpr& binary) -> Value {
              const auto lhs = as_int(predicate_expr(function, values, *binary.lhs));
              const auto rhs = as_int(predicate_expr(function, values, *binary.rhs));
              return arithmetic(binary.op, lhs, rhs);
            },
            [&values](const funnyir::IrPredicateValue& ref) -> Value { return values.at(ref.id); },
        },
        expr.node);
  }

  bool predicate(const funnyir::IrFunction& function, const std::vector<Value>& values,
                 const funnyir::IrPredicate& pred) {
    return std::visit(
        funny::Overloaded{
            [](const funnyir::IrBoolLiteral& literal) { return literal.value; },
            [this, &function, &values](const funnyir::IrCompare& compare_node) {
              const auto lhs = as_int(predicate_expr(function, values, *compare_node.lhs));
              const auto rhs = as_int(predicate_expr(function, values, *compare_node.rhs));
              return compare(compare_node.op, lhs, rhs);
            },
            [this, &function, &values](const funnyir::IrNot& node) {
              return !predicate(function, values, *node.operand);
            },
            [this, &function, &values](const funnyir::IrLogical& node) {
              const bool lhs = predicate(function, values, *node.lhs);
              const bool rhs = predicate(function, values, *node.rhs);
              if (node.op == funny::LogicOp::And) return lhs && rhs;
              if (node.op == funny::LogicOp::Or) return lhs || rhs;
              return !lhs || rhs;
            },
            [](const funnyir::IrQuantifier&) -> bool { throw std::runtime_error("quantifier not supported by test interpreter"); },
            [](const funnyir::IrFormulaRef&) -> bool { throw std::runtime_error("formula ref not supported by test interpreter"); },
        },
        pred.node);
  }

  ExecutionResult invoke(std::string_view name, const std::vector<Value>& args, std::size_t depth) {
    if (depth > 64) throw std::runtime_error("call depth exceeded");
    const auto& function = find_function(name);
    if (function.params.size() != args.size()) throw std::runtime_error("invalid call arity");
    std::vector<Value> values(function.values.size(), std::int32_t{0});
    for (std::size_t i = 0; i < args.size(); ++i) values.at(function.params[i].value) = args[i];

    funnyir::BlockId current = function.entry;
    for (;;) {
      if (++steps_ > 100000) throw std::runtime_error("step limit exceeded");
      const auto& block = function.blocks.at(current);
      for (const auto& instruction : block.instructions) execute_instruction(function, values, instruction, depth);
      if (const auto* jump = std::get_if<funnyir::JumpTerm>(&block.terminator)) {
        current = jump->target;
        continue;
      }
      if (const auto* branch = std::get_if<funnyir::BranchTerm>(&block.terminator)) {
        current = predicate(function, values, branch->condition) ? branch->true_target : branch->false_target;
        continue;
      }
      const auto& ret = std::get<funnyir::ReturnTerm>(block.terminator);
      std::vector<Value> results;
      for (funnyir::ValueId id : ret.values) results.push_back(values.at(id));
      return {std::move(results), false, {}};
    }
  }

  void execute_instruction(const funnyir::IrFunction& function, std::vector<Value>& values,
                           const funnyir::IrInstruction& instruction, std::size_t depth) {
    const auto read = [&values](const funnyir::IrOperand& item) -> Value {
      return std::visit(funny::Overloaded{[&values](funnyir::ValueRef ref) { return values.at(ref.id); },
                                          [](std::int64_t integer) -> Value { return from_literal(integer); }},
                         item);
    };
    auto set_result = [&values, &instruction](std::size_t n, Value value) { values.at(instruction.defs.at(n)) = std::move(value); };
    switch (instruction.opcode) {
      case funnyir::IrOpcode::Constant:
        set_result(0, from_literal(std::get<std::int64_t>(instruction.operands.at(0))));
        break;
      case funnyir::IrOpcode::Copy:
        set_result(0, read(instruction.operands.at(0)));
        break;
      case funnyir::IrOpcode::Neg:
        set_result(0, wrap_int(0U - int_bits(as_int(read(instruction.operands.at(0))))));
        break;
      case funnyir::IrOpcode::Add:
      case funnyir::IrOpcode::Sub:
      case funnyir::IrOpcode::Mul:
      case funnyir::IrOpcode::Div: {
        const std::int32_t lhs = as_int(read(instruction.operands.at(0)));
        const std::int32_t rhs = as_int(read(instruction.operands.at(1)));
        funny::BinOp op = funny::BinOp::Add;
        if (instruction.opcode == funnyir::IrOpcode::Sub) op = funny::BinOp::Sub;
        if (instruction.opcode == funnyir::IrOpcode::Mul) op = funny::BinOp::Mul;
        if (instruction.opcode == funnyir::IrOpcode::Div) op = funny::BinOp::Div;
        set_result(0, arithmetic(op, lhs, rhs));
        break;
      }
      case funnyir::IrOpcode::LoadElement: {
        const auto& array = as_array(read(instruction.operands.at(0)));
        const std::int32_t index = as_int(read(instruction.operands.at(1)));
        if (index < 0 || static_cast<std::size_t>(index) >= array.size()) throw std::runtime_error("array bounds");
        set_result(0, array[static_cast<std::size_t>(index)]);
        break;
      }
      case funnyir::IrOpcode::ArraySet: {
        auto array = as_array(read(instruction.operands.at(0)));
        const std::int32_t index = as_int(read(instruction.operands.at(1)));
        if (index < 0 || static_cast<std::size_t>(index) >= array.size()) throw std::runtime_error("array bounds");
        array[static_cast<std::size_t>(index)] = as_int(read(instruction.operands.at(2)));
        set_result(0, std::move(array));
        break;
      }
      case funnyir::IrOpcode::Call: {
        std::vector<Value> args;
        for (const funnyir::IrOperand& item : instruction.operands) args.push_back(read(item));
        if (instruction.callee == "length") {
          if (args.size() != 1) throw std::runtime_error("invalid length arity");
          set_result(0, static_cast<std::int32_t>(as_array(args.front()).size()));
          break;
        }
        const ExecutionResult result = invoke(instruction.callee, args, depth + 1);
        if (result.trapped) throw std::runtime_error(result.error);
        if (result.results.size() != instruction.defs.size()) throw std::runtime_error("invalid call result count");
        for (std::size_t i = 0; i < result.results.size(); ++i) set_result(i, result.results[i]);
        break;
      }
      case funnyir::IrOpcode::Assert:
        if (!instruction.predicate || !predicate(function, values, *instruction.predicate)) {
          throw std::runtime_error("assert failed");
        }
        break;
      case funnyir::IrOpcode::Assume: break;
    }
  }
};

}  // namespace

ExecutionResult execute(const funnyir::IrModule& module, std::string_view function, const std::vector<Value>& args) {
  return Interpreter(module).run(function, args);
}

}  // namespace funnyir_test
