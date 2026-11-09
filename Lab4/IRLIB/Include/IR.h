#ifndef FUNNYIR_IR_H
#define FUNNYIR_IR_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "Ast.h"

namespace funnyir {

using ValueId = std::size_t;
using BlockId = std::size_t;
using IrType = funny::TypeKind;

enum class IrValueKind { Parameter, Local, Return, Temporary };
enum class IrOpcode { Constant, Copy, Neg, Add, Sub, Mul, Div, LoadElement, ArraySet, Call, Assert, Assume };

struct IrValue {
  ValueId id = 0;
  IrType type = IrType::Int;
  IrValueKind kind = IrValueKind::Temporary;
  std::string name;
  std::optional<funny::VarKind> symbol_kind;
};

struct ValueRef {
  ValueId id = 0;
};
using IrOperand = std::variant<ValueRef, std::int64_t>;

struct IrPredicateExpr;
using IrPredicateExprPtr = std::shared_ptr<IrPredicateExpr>;
struct IrIntLiteral {
  std::string text;
};
struct IrVarRef {
  std::string name;
  funny::VarKind kind = funny::VarKind::Unresolved;
};
struct IrArrayAccess {
  std::string array;
  IrPredicateExprPtr index;
};
struct IrCallExpr {
  std::string callee;
  std::vector<IrPredicateExprPtr> args;
};
struct IrNegExpr {
  IrPredicateExprPtr operand;
};
struct IrBinaryExpr {
  funny::BinOp op = funny::BinOp::Add;
  IrPredicateExprPtr lhs;
  IrPredicateExprPtr rhs;
};
struct IrPredicateValue {
  ValueId id = 0;
};
struct IrPredicateExpr {
  funny::Pos pos;
  IrType type = IrType::Int;
  std::variant<IrIntLiteral, IrVarRef, IrArrayAccess, IrCallExpr, IrNegExpr, IrBinaryExpr, IrPredicateValue> node;
};

struct IrPredicate;
using IrPredicatePtr = std::shared_ptr<IrPredicate>;
struct IrBoolLiteral {
  bool value = false;
};
struct IrCompare {
  funny::CmpOp op = funny::CmpOp::Eq;
  IrPredicateExprPtr lhs;
  IrPredicateExprPtr rhs;
};
struct IrNot {
  IrPredicatePtr operand;
};
struct IrLogical {
  funny::LogicOp op = funny::LogicOp::And;
  IrPredicatePtr lhs;
  IrPredicatePtr rhs;
};
struct IrQuantifier {
  bool is_forall = false;
  std::string name;
  IrType type = IrType::Int;
  IrPredicatePtr body;
};
struct IrFormulaRef {
  std::string name;
  std::vector<IrPredicateExprPtr> args;
};
struct IrPredicate {
  funny::Pos pos;
  std::variant<IrBoolLiteral, IrCompare, IrNot, IrLogical, IrQuantifier, IrFormulaRef> node;
};

struct IrInstruction {
  IrOpcode opcode = IrOpcode::Constant;
  funny::Pos pos;
  std::vector<ValueId> defs;
  std::vector<ValueId> uses;
  std::vector<IrOperand> operands;
  std::string callee;
  std::optional<IrPredicate> predicate;
};

struct JumpTerm {
  BlockId target = 0;
};
struct BranchTerm {
  IrPredicate condition;
  BlockId true_target = 0;
  BlockId false_target = 0;
  std::vector<ValueId> uses;
};
struct ReturnTerm {
  std::vector<ValueId> values;
};
using IrTerminator = std::variant<JumpTerm, BranchTerm, ReturnTerm>;

struct IrBlock {
  BlockId id = 0;
  std::vector<IrInstruction> instructions;
  IrTerminator terminator = ReturnTerm{};
  std::vector<BlockId> successors;
  std::vector<BlockId> predecessors;
  std::optional<IrPredicate> loop_invariant;
};

struct IrParameter {
  std::string name;
  IrType type = IrType::Int;
  ValueId value = 0;
};
struct IrFunction {
  std::string name;
  std::vector<IrParameter> params;
  std::vector<IrParameter> returns;
  std::vector<IrValue> values;
  std::vector<IrBlock> blocks;
  BlockId entry = 0;
  BlockId exit = 0;
  std::optional<IrPredicate> requires_clause;
  std::optional<IrPredicate> ensures_clause;
};
struct IrFormula {
  std::string name;
  std::vector<IrParameter> params;
  IrPredicate body;
};
enum class IrDeclKind { Function, Formula };
struct IrDeclRef {
  IrDeclKind kind = IrDeclKind::Function;
  std::size_t index = 0;
};
struct IrModule {
  std::vector<IrFunction> functions;
  std::vector<IrFormula> formulas;
  std::vector<IrDeclRef> declaration_order;
};

std::vector<BlockId> successors(const IrTerminator& terminator);
void rebuild_cfg(IrFunction& function);

}  // namespace funnyir

#endif  // FUNNYIR_IR_H
