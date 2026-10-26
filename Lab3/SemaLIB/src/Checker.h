// Проверка имён и типов одной функции или формулы; заполняет HW3-аннотации AST.

#ifndef FUNNYSEMA_CHECKER_H
#define FUNNYSEMA_CHECKER_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Ast.h"
#include "Diagnostic.h"
#include "Symbols.h"

namespace funny {

// Тип во время проверки: конкретный, «тип локальной vars_[var] без аннотации» (Var) или ошибка.
// Ошибка совместима с чем угодно — так одна ошибка не порождает каскад сообщений.
struct Ty {
  enum class Kind { Error, Int, IntArray, Var };
  Kind kind = Kind::Error;
  int var = -1;

  static Ty of(TypeKind t) { return {t == TypeKind::Int ? Kind::Int : Kind::IntArray, -1}; }
  static Ty variable(int v) { return {Kind::Var, v}; }
};

struct VarSymbol {
  std::string name;
  VarKind kind;
  std::optional<TypeKind> type;  // пусто — тип локальной ещё не выведен
  Pos pos;
  std::size_t id;          // индекс в vars_ и init_
  bool inferred = false;   // локальная без аннотации: тип выводится
  Pos inferred_at{};       // где тип выведен
};


std::string plural(std::size_t n, std::string_view one, std::string_view few, std::string_view many);

class Checker {
 public:
  Checker(const GlobalScope& globals, std::vector<Diagnostic>& diags) : globals_(globals), diags_(diags) {}

  void function(Function& f);
  void formula(Formula& f);

 private:
  
  struct Context {
    unsigned visible;
    std::string_view where;  
    std::string_view hint;   
    bool logic;              
    bool in_body;            
  };

  // ---- области видимости ----
  void begin_scope();
  void declare(const std::string& name, VarKind kind, std::optional<TypeKind> type, Pos pos);
  const VarSymbol* find_var(std::string_view name) const;
  const VarSymbol* lookup(const std::string& name, Pos pos);  // с сообщениями «неизвестное/недоступно»
  const VarSymbol* assign_target(const Name& target);         // + «параметр только для чтения»
  const GlobalSymbol* callee(const std::string& name, Pos pos, bool want_formula);

  // ---- типы ----
  Ty type_of(const VarSymbol& s) const;
  std::optional<TypeKind> resolve(Ty t) const;
  void infer(int var, TypeKind t, Pos at);
  std::string describe(Ty t) const;
  bool expect(Ty got, TypeKind want, Pos at, const std::string& what);
  void assign_types(const VarSymbol& target, Ty value, Pos at);

  // ---- выражения и предикаты ----
  Ty expr(Expr& e);
  Ty read_var(const std::string& name, Pos pos, VarKind* kind);
  Ty int_lit(const IntLit& n, Pos pos, bool negated);
  Ty call(Call& c, Pos pos);
  void args(const GlobalSymbol* g, std::vector<ExprPtr>& as, Pos pos);
  Ty array_update(Expr& value);
  void cond(Cond& c);
  void with_context(Context ctx, Cond& c);

  // ---- операторы ----
  void stmt(Stmt& s);
  void assign(Assign& a);
  void tuple_assign(TupleAssign& t);

  void error(Pos pos, std::string message) { diags_.push_back({pos, std::move(message)}); }
  void annotate(Expr& e, Ty t) const;

  const GlobalScope& globals_;
  std::vector<Diagnostic>& diags_;

  std::vector<VarSymbol> vars_;
  std::vector<VarSymbol> bound_;
  std::vector<bool> init_;
  Context ctx_{};
};

}  // namespace funny

#endif  // FUNNYSEMA_CHECKER_H
