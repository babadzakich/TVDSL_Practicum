// AST языка Funny. Каждый узел хранит позицию своего первого токена.
// Поля с пометкой «HW3» заполняет семантический анализ (SemaLIB); парсер их не трогает.

#ifndef FUNNYPARSE_AST_H
#define FUNNYPARSE_AST_H

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "Diagnostic.h"

namespace funny {

enum class TypeKind { Int, IntArray };

// HW3: тип значения выражения. Пустой — тип не вычислен (до проверки или из-за ошибки);
// один элемент — int/int[]; несколько — кортеж (только вызов функции в TupleAssign).
using ExprType = std::vector<TypeKind>;

// HW3: к какому объявлению относится имя переменной.
enum class VarKind { Unresolved, Param, Return, Local, Bound };  // Bound — переменная квантора

struct Name {
  std::string text;
  Pos pos;
};

struct VarDef {
  std::string name;
  TypeKind type;
  Pos pos;
};

struct LocalDef {
  std::string name;
  std::optional<TypeKind> type;  // nullopt — тип выводится позже
  Pos pos;
  bool inferred = false;  // HW3: type выведен, а не указан в исходнике
};

// ---- выражения ----

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

struct IntLit {
  std::string text;  // лексема как в исходнике
};
struct VarRef {
  std::string name;
  VarKind kind = VarKind::Unresolved;  // HW3
};
struct ArrayAccess {
  std::string array;
  ExprPtr index;
};
struct Call {
  std::string callee;
  std::vector<ExprPtr> args;
};
struct Neg {
  ExprPtr operand;
};
enum class BinOp { Add, Sub, Mul, Div };
struct Binary {
  BinOp op;
  ExprPtr lhs, rhs;
};

struct Expr {
  Pos pos;
  std::variant<IntLit, VarRef, ArrayAccess, Call, Neg, Binary> node;
  ExprType type;  // HW3
};

// ---- условия и предикаты (общий набор узлов) ----

struct Cond;
using CondPtr = std::unique_ptr<Cond>;

enum class CmpOp { Eq, Ne, Ge, Le, Gt, Lt };
enum class LogicOp { And, Or, Implies };

struct BoolLit {
  bool value;
};
struct Compare {
  CmpOp op;
  ExprPtr lhs, rhs;
};
struct Not {
  CondPtr operand;
};
struct Logical {
  LogicOp op;
  CondPtr lhs, rhs;
};
struct Quantifier {
  bool is_forall;
  VarDef var;
  CondPtr body;
};
struct FormulaRef {
  std::string name;
  std::vector<ExprPtr> args;
};

struct Cond {
  Pos pos;
  std::variant<BoolLit, Compare, Not, Logical, Quantifier, FormulaRef> node;
};

// ---- операторы ----

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;

enum class Sugar { None, ArrayUpdate };

struct Block {
  std::vector<StmtPtr> body;
};
struct If {
  CondPtr cond;
  StmtPtr then_branch;
  StmtPtr else_branch;  // может быть null
};
struct While {
  CondPtr cond;
  CondPtr invariant;  // может быть null
  StmtPtr body;
};
struct Assert {
  CondPtr pred;
};
struct Assume {
  CondPtr pred;
};
// a[i] = v хранится как a = set(a, i, v) с sugar = ArrayUpdate.
struct Assign {
  Name target;
  ExprPtr value;
  Sugar sugar = Sugar::None;
};
struct TupleAssign {
  std::vector<Name> targets;
  ExprPtr call;  // всегда Call
};

struct Stmt {
  Pos pos;
  std::variant<Block, If, While, Assert, Assume, Assign, TupleAssign> node;
};

// ---- объявления ----

struct Function {
  Name name;
  std::vector<VarDef> params;
  CondPtr requires_clause;  // может быть null
  std::vector<VarDef> returns;
  CondPtr ensures_clause;  // может быть null
  std::vector<LocalDef> uses;
  StmtPtr body;
};

struct Formula {
  Name name;
  std::vector<VarDef> params;
  CondPtr body;
};

using Decl = std::variant<Function, Formula>;

struct Module {
  std::vector<Decl> decls;
};

ExprPtr clone(const Expr& e);

std::string_view to_string(BinOp op);  // "+" "-" "*" "/"
std::string_view to_string(CmpOp op);  // "==" "!=" ">=" "<=" ">" "<"
std::string to_string(TypeKind t);     // "int" "int[]"
std::string to_string(const ExprType& t);  // "int", "(int, int[])"; пустой — "?"
std::string_view to_string(VarKind k);     // "param" "return" "local" "bound"; Unresolved — ""

}  // namespace funny

#endif  // FUNNYPARSE_AST_H
