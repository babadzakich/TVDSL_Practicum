#include "Lowering.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include "Overloaded.h"

namespace funnyir {
namespace {

IrType single_type(const funny::Expr& expr) {
  return expr.type.empty() ? IrType::Int : expr.type.front();
}

IrPredicateExprPtr value_expr(ValueId value, funny::Pos pos) {
  auto result = std::make_shared<IrPredicateExpr>();
  result->pos = pos;
  result->node = IrPredicateValue{value};
  return result;
}

IrPredicateExprPtr copy_expr(const funny::Expr& expr) {
  auto result = std::make_shared<IrPredicateExpr>();
  result->pos = expr.pos;
  result->type = single_type(expr);
  result->node = std::visit(
      funny::Overloaded{
          [](const funny::IntLit& node) -> decltype(IrPredicateExpr::node) { return IrIntLiteral{node.text}; },
          [](const funny::VarRef& node) -> decltype(IrPredicateExpr::node) {
            return IrVarRef{node.name, node.kind};
          },
          [](const funny::ArrayAccess& node) -> decltype(IrPredicateExpr::node) {
            return IrArrayAccess{node.array, copy_expr(*node.index)};
          },
          [](const funny::Call& node) -> decltype(IrPredicateExpr::node) {
            IrCallExpr call;
            call.callee = node.callee;
            for (const auto& arg : node.args) call.args.push_back(copy_expr(*arg));
            return call;
          },
          [](const funny::Neg& node) -> decltype(IrPredicateExpr::node) { return IrNegExpr{copy_expr(*node.operand)}; },
          [](const funny::Binary& node) -> decltype(IrPredicateExpr::node) {
            return IrBinaryExpr{node.op, copy_expr(*node.lhs), copy_expr(*node.rhs)};
          },
      },
      expr.node);
  return result;
}

IrPredicate copy_predicate(const funny::Cond& cond) {
  IrPredicate result;
  result.pos = cond.pos;
  result.node = std::visit(
      funny::Overloaded{
          [](const funny::BoolLit& node) -> decltype(IrPredicate::node) { return IrBoolLiteral{node.value}; },
          [](const funny::Compare& node) -> decltype(IrPredicate::node) {
            return IrCompare{node.op, copy_expr(*node.lhs), copy_expr(*node.rhs)};
          },
          [](const funny::Not& node) -> decltype(IrPredicate::node) {
            return IrNot{std::make_shared<IrPredicate>(copy_predicate(*node.operand))};
          },
          [](const funny::Logical& node) -> decltype(IrPredicate::node) {
            return IrLogical{node.op, std::make_shared<IrPredicate>(copy_predicate(*node.lhs)),
                             std::make_shared<IrPredicate>(copy_predicate(*node.rhs))};
          },
          [](const funny::Quantifier& node) -> decltype(IrPredicate::node) {
            return IrQuantifier{node.is_forall, node.var.name, node.var.type,
                                std::make_shared<IrPredicate>(copy_predicate(*node.body))};
          },
          [](const funny::FormulaRef& node) -> decltype(IrPredicate::node) {
            IrFormulaRef ref;
            ref.name = node.name;
            for (const auto& arg : node.args) ref.args.push_back(copy_expr(*arg));
            return ref;
          },
      },
      cond.node);
  return result;
}

void collect_expr_uses(const IrPredicateExpr& expr, const std::unordered_map<std::string, ValueId>& slots,
                       std::vector<ValueId>& uses) {
  std::visit(
      funny::Overloaded{
          [](const IrIntLiteral&) {},
          [&slots, &uses](const IrVarRef& ref) {
            if (ref.kind != funny::VarKind::Bound) {
              if (const auto found = slots.find(ref.name); found != slots.end()) uses.push_back(found->second);
            }
          },
          [&slots, &uses](const IrArrayAccess& access) {
            if (const auto found = slots.find(access.array); found != slots.end()) uses.push_back(found->second);
            collect_expr_uses(*access.index, slots, uses);
          },
          [&slots, &uses](const IrCallExpr& call) {
            for (const auto& arg : call.args) collect_expr_uses(*arg, slots, uses);
          },
          [&slots, &uses](const IrNegExpr& neg) { collect_expr_uses(*neg.operand, slots, uses); },
          [&slots, &uses](const IrBinaryExpr& binary) {
            collect_expr_uses(*binary.lhs, slots, uses);
            collect_expr_uses(*binary.rhs, slots, uses);
          },
          [&uses](const IrPredicateValue& value) { uses.push_back(value.id); },
      },
      expr.node);
}

void collect_predicate_uses(const IrPredicate& predicate, const std::unordered_map<std::string, ValueId>& slots,
                            std::vector<ValueId>& uses) {
  std::visit(
      funny::Overloaded{
          [](const IrBoolLiteral&) {},
          [&slots, &uses](const IrCompare& compare) {
            collect_expr_uses(*compare.lhs, slots, uses);
            collect_expr_uses(*compare.rhs, slots, uses);
          },
          [&slots, &uses](const IrNot& node) { collect_predicate_uses(*node.operand, slots, uses); },
          [&slots, &uses](const IrLogical& node) {
            collect_predicate_uses(*node.lhs, slots, uses);
            collect_predicate_uses(*node.rhs, slots, uses);
          },
          [&slots, &uses](const IrQuantifier& node) { collect_predicate_uses(*node.body, slots, uses); },
          [&slots, &uses](const IrFormulaRef& node) {
            for (const auto& arg : node.args) collect_expr_uses(*arg, slots, uses);
          },
      },
      predicate.node);
}

class FunctionLowerer {
 public:
  explicit FunctionLowerer(const funny::Function& source) : source_(source) {}

  IrFunction run() {
    result_.name = source_.name.text;
    for (const funny::VarDef& param : source_.params) {
      const ValueId value = add_slot(param.name, param.type, IrValueKind::Parameter, funny::VarKind::Param);
      result_.params.push_back({param.name, param.type, value});
    }
    for (const funny::VarDef& ret : source_.returns) {
      const ValueId value = add_slot(ret.name, ret.type, IrValueKind::Return, funny::VarKind::Return);
      result_.returns.push_back({ret.name, ret.type, value});
    }
    for (const funny::LocalDef& local : source_.uses) {
      add_slot(local.name, local.type.value_or(IrType::Int), IrValueKind::Local, funny::VarKind::Local);
    }
    if (source_.requires_clause) result_.requires_clause = copy_predicate(*source_.requires_clause);
    if (source_.ensures_clause) result_.ensures_clause = copy_predicate(*source_.ensures_clause);

    current_ = add_block();
    result_.entry = current_;
    lower_stmt(*source_.body);
    result_.exit = current_;
    ReturnTerm ret;
    for (const IrParameter& value : result_.returns) ret.values.push_back(value.value);
    block(current_).terminator = std::move(ret);
    rebuild_cfg(result_);
    return std::move(result_);
  }

 private:
  const funny::Function& source_;
  IrFunction result_;
  std::unordered_map<std::string, ValueId> slots_;
  BlockId current_ = 0;

  IrBlock& block(BlockId id) { return result_.blocks.at(id); }

  BlockId add_block() {
    const BlockId id = result_.blocks.size();
    IrBlock next;
    next.id = id;
    result_.blocks.push_back(std::move(next));
    return id;
  }

  ValueId add_slot(const std::string& name, IrType type, IrValueKind kind, funny::VarKind symbol) {
    const ValueId id = result_.values.size();
    result_.values.push_back({id, type, kind, name, symbol});
    slots_[name] = id;
    return id;
  }

  ValueId add_temp(IrType type) {
    const ValueId id = result_.values.size();
    result_.values.push_back({id, type, IrValueKind::Temporary, "t" + std::to_string(id), std::nullopt});
    return id;
  }

  ValueId slot(const std::string& name) const {
    const auto found = slots_.find(name);
    if (found == slots_.end()) throw std::logic_error("аннотированная переменная отсутствует в IR: " + name);
    return found->second;
  }

  void emit(IrOpcode opcode, funny::Pos pos, std::vector<ValueId> defs, std::vector<ValueId> uses,
            std::vector<IrOperand> operands = {}, std::string callee = {}, std::optional<IrPredicate> predicate = {}) {
    block(current_).instructions.push_back(
        IrInstruction{opcode, pos, std::move(defs), std::move(uses), std::move(operands), std::move(callee),
                      std::move(predicate)});
  }

  std::vector<ValueId> lower_args(const std::vector<funny::ExprPtr>& args) {
    std::vector<ValueId> values;
    values.reserve(args.size());
    for (const auto& arg : args) values.push_back(lower_expr(*arg));
    return values;
  }

  ValueId lower_call(const funny::Call& call, funny::Pos pos, IrType result_type,
                     const std::vector<ValueId>& destinations = {}) {
    const auto args = lower_args(call.args);
    std::vector<ValueId> defs = destinations;
    if (defs.empty()) defs.push_back(add_temp(result_type));
    std::vector<IrOperand> operands;
    for (ValueId arg : args) operands.push_back(ValueRef{arg});
    emit(IrOpcode::Call, pos, defs, args, std::move(operands), call.callee);
    return defs.front();
  }

  ValueId lower_expr(const funny::Expr& expr) {
    return std::visit(
        funny::Overloaded{
            [&expr, this](const funny::IntLit& literal) {
              const std::int64_t value = std::stoll(literal.text);
              const ValueId dst = add_temp(IrType::Int);
              emit(IrOpcode::Constant, expr.pos, {dst}, {}, {value});
              return dst;
            },
            [this](const funny::VarRef& ref) { return slot(ref.name); },
            [&expr, this](const funny::ArrayAccess& access) {
              const ValueId array = slot(access.array);
              const ValueId index = lower_expr(*access.index);
              const ValueId dst = add_temp(IrType::Int);
              emit(IrOpcode::LoadElement, expr.pos, {dst}, {array, index}, {ValueRef{array}, ValueRef{index}});
              return dst;
            },
            [&expr, this](const funny::Call& call) { return lower_call(call, expr.pos, single_type(expr)); },
            [&expr, this](const funny::Neg& neg) {
              if (const auto* literal = std::get_if<funny::IntLit>(&neg.operand->node);
                  literal != nullptr && literal->text == "2147483648") {
                const ValueId dst = add_temp(IrType::Int);
                emit(IrOpcode::Constant, expr.pos, {dst}, {}, {static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min())});
                return dst;
              }
              const ValueId value = lower_expr(*neg.operand);
              const ValueId dst = add_temp(IrType::Int);
              emit(IrOpcode::Neg, expr.pos, {dst}, {value}, {ValueRef{value}});
              return dst;
            },
            [&expr, this](const funny::Binary& binary) {
              const ValueId lhs = lower_expr(*binary.lhs);
              const ValueId rhs = lower_expr(*binary.rhs);
              const ValueId dst = add_temp(IrType::Int);
              IrOpcode opcode = IrOpcode::Add;
              switch (binary.op) {
                case funny::BinOp::Add: opcode = IrOpcode::Add; break;
                case funny::BinOp::Sub: opcode = IrOpcode::Sub; break;
                case funny::BinOp::Mul: opcode = IrOpcode::Mul; break;
                case funny::BinOp::Div: opcode = IrOpcode::Div; break;
              }
              emit(opcode, expr.pos, {dst}, {lhs, rhs}, {ValueRef{lhs}, ValueRef{rhs}});
              return dst;
            },
        },
        expr.node);
  }

  IrPredicate lower_condition(const funny::Cond& cond) {
    IrPredicate result;
    result.pos = cond.pos;
    result.node = std::visit(
        funny::Overloaded{
            [](const funny::BoolLit& node) -> decltype(IrPredicate::node) { return IrBoolLiteral{node.value}; },
            [this](const funny::Compare& node) -> decltype(IrPredicate::node) {
              const ValueId lhs = lower_expr(*node.lhs);
              const ValueId rhs = lower_expr(*node.rhs);
              return IrCompare{node.op, value_expr(lhs, node.lhs->pos), value_expr(rhs, node.rhs->pos)};
            },
            [this](const funny::Not& node) -> decltype(IrPredicate::node) {
              return IrNot{std::make_shared<IrPredicate>(lower_condition(*node.operand))};
            },
            [this](const funny::Logical& node) -> decltype(IrPredicate::node) {
              auto lhs = std::make_shared<IrPredicate>(lower_condition(*node.lhs));
              auto rhs = std::make_shared<IrPredicate>(lower_condition(*node.rhs));
              return IrLogical{node.op, std::move(lhs), std::move(rhs)};
            },
            [&cond](const funny::Quantifier&) -> decltype(IrPredicate::node) { return copy_predicate(cond).node; },
            [&cond](const funny::FormulaRef&) -> decltype(IrPredicate::node) { return copy_predicate(cond).node; },
        },
        cond.node);
    return result;
  }

  void lower_assign(const funny::Assign& assign, funny::Pos pos) {
    const ValueId target = slot(assign.target.text);
    if (assign.sugar == funny::Sugar::ArrayUpdate) {
      const auto* call = std::get_if<funny::Call>(&assign.value->node);
      if (call == nullptr || call->args.size() != 3) throw std::logic_error("неверное AST обновления массива");
      const ValueId array = target;
      const ValueId index = lower_expr(*call->args[1]);
      const ValueId value = lower_expr(*call->args[2]);
      emit(IrOpcode::ArraySet, pos, {target}, {array, index, value},
           {ValueRef{array}, ValueRef{index}, ValueRef{value}});
      return;
    }
    const ValueId value = lower_expr(*assign.value);
    emit(IrOpcode::Copy, pos, {target}, {value}, {ValueRef{value}});
  }

  void lower_stmt(const funny::Stmt& stmt) {
    std::visit(
        funny::Overloaded{
            [this](const funny::Block& body) {
              for (const auto& child : body.body) lower_stmt(*child);
            },
            [this](const funny::If& node) {
              IrPredicate condition = lower_condition(*node.cond);
              const BlockId then_block = add_block();
              const BlockId else_block = add_block();
              const BlockId join_block = add_block();
              std::vector<ValueId> uses;
              collect_predicate_uses(condition, slots_, uses);
              block(current_).terminator = BranchTerm{std::move(condition), then_block, else_block, std::move(uses)};

              current_ = then_block;
              lower_stmt(*node.then_branch);
              block(current_).terminator = JumpTerm{join_block};

              current_ = else_block;
              if (node.else_branch) lower_stmt(*node.else_branch);
              block(current_).terminator = JumpTerm{join_block};
              current_ = join_block;
            },
            [this](const funny::While& node) {
              const BlockId header = add_block();
              const BlockId body_block = add_block();
              const BlockId exit = add_block();
              block(current_).terminator = JumpTerm{header};

              current_ = header;
              if (node.invariant) block(header).loop_invariant = copy_predicate(*node.invariant);
              IrPredicate condition = lower_condition(*node.cond);
              std::vector<ValueId> uses;
              collect_predicate_uses(condition, slots_, uses);
              block(header).terminator = BranchTerm{std::move(condition), body_block, exit, std::move(uses)};

              current_ = body_block;
              lower_stmt(*node.body);
              block(current_).terminator = JumpTerm{header};
              current_ = exit;
            },
            [this, &stmt](const funny::Assert& node) {
              auto predicate = copy_predicate(*node.pred);
              std::vector<ValueId> uses;
              collect_predicate_uses(predicate, slots_, uses);
              emit(IrOpcode::Assert, stmt.pos, {}, std::move(uses), {}, {}, std::move(predicate));
            },
            [this, &stmt](const funny::Assume& node) {
              auto predicate = copy_predicate(*node.pred);
              std::vector<ValueId> uses;
              collect_predicate_uses(predicate, slots_, uses);
              emit(IrOpcode::Assume, stmt.pos, {}, std::move(uses), {}, {}, std::move(predicate));
            },
            [this, &stmt](const funny::Assign& node) { lower_assign(node, stmt.pos); },
            [this, &stmt](const funny::TupleAssign& node) {
              const auto* call = std::get_if<funny::Call>(&node.call->node);
              if (call == nullptr) throw std::logic_error("TupleAssign не содержит Call");
              std::vector<ValueId> destinations;
              for (const funny::Name& target : node.targets) destinations.push_back(slot(target.text));
              lower_call(*call, stmt.pos, IrType::Int, destinations);
            },
        },
        stmt.node);
  }
};

IrFormula lower_formula(const funny::Formula& formula) {
  IrFormula result;
  result.name = formula.name.text;
  for (std::size_t i = 0; i < formula.params.size(); ++i) {
    result.params.push_back({formula.params[i].name, formula.params[i].type, i});
  }
  result.body = copy_predicate(*formula.body);
  return result;
}

}  // namespace

IrModule lower(const funny::Module& module) {
  IrModule result;
  for (const funny::Decl& decl : module.decls) {
    std::visit(
        funny::Overloaded{
            [&result](const funny::Function& function) {
              result.declaration_order.push_back({IrDeclKind::Function, result.functions.size()});
              result.functions.push_back(FunctionLowerer(function).run());
            },
            [&result](const funny::Formula& formula) {
              result.declaration_order.push_back({IrDeclKind::Formula, result.formulas.size()});
              result.formulas.push_back(lower_formula(formula));
            },
        },
        decl);
  }
  return result;
}

}  // namespace funnyir
