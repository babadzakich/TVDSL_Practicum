// Выражения, вызовы, условия и предикаты.

#include "Checker.h"
#include "Overloaded.h"

namespace funny {

namespace {

bool fits_int32(const std::string& digits, bool negated) {
  const std::string_view limit = negated ? "2147483648" : "2147483647";
  return digits.size() < limit.size() || (digits.size() == limit.size() && digits <= limit);
}

std::string callee_noun(const GlobalSymbol& g, bool genitive) {
  if (g.kind == GlobalKind::Formula) {
    return std::string(genitive ? "формулы '" : "формула '") + g.name + "'";
  }
  return std::string(genitive ? "функции '" : "функция '") + g.name + "'";
}

}  // namespace

std::string plural(std::size_t n, std::string_view one, std::string_view few, std::string_view many) {
  const std::size_t mod100 = n % 100;
  const std::size_t mod10 = n % 10;
  std::string_view word = many;
  if (mod100 < 11 || mod100 > 14) {
    if (mod10 == 1) {
      word = one;
    } else if (mod10 >= 2 && mod10 <= 4) {
      word = few;
    }
  }
  return std::to_string(n) + ' ' + std::string(word);
}

Ty Checker::expr(Expr& e) {
  const Ty t = std::visit(Overloaded{
                              [&](IntLit& n) { return int_lit(n, e.pos, false); },
                              [&](VarRef& v) { return read_var(v.name, e.pos, &v.kind); },
                              [&](ArrayAccess& a) {
                                const Ty arr = read_var(a.array, e.pos, nullptr);
                                expect(arr, TypeKind::IntArray, e.pos, "обращение по индексу к '" + a.array + "'");
                                expect(expr(*a.index), TypeKind::Int, a.index->pos, "индекс массива");
                                return Ty::of(TypeKind::Int);
                              },
                              [&](Call& c) { return call(c, e.pos); },
                              [&](Neg& n) {
                                if (auto* lit = std::get_if<IntLit>(&n.operand->node)) {
                                  annotate(*n.operand, int_lit(*lit, n.operand->pos, true));
                                } else {
                                  expect(expr(*n.operand), TypeKind::Int, n.operand->pos, "операнд унарного '-'");
                                }
                                return Ty::of(TypeKind::Int);
                              },
                              [&](Binary& b) {
                                const std::string what = "операнд '" + std::string(to_string(b.op)) + "'";
                                expect(expr(*b.lhs), TypeKind::Int, b.lhs->pos, what);
                                expect(expr(*b.rhs), TypeKind::Int, b.rhs->pos, what);
                                return Ty::of(TypeKind::Int);
                              },
                          },
                          e.node);
  annotate(e, t);
  return t;
}

Ty Checker::read_var(const std::string& name, Pos pos, VarKind* kind) {
  const VarSymbol* s = lookup(name, pos);
  if (s == nullptr) {
    return {};
  }
  if (kind != nullptr) {
    *kind = s->kind;
  }
  const bool tracked = s->kind == VarKind::Return || s->kind == VarKind::Local;
  if (ctx_.in_body && tracked && !init_[s->id]) {
    error(pos, "переменная '" + name + "' используется до присваивания");
    init_[s->id] = true;  // одно сообщение на переменную
  }
  return type_of(*s);
}

Ty Checker::int_lit(const IntLit& n, Pos pos, bool negated) {
  if (!ctx_.logic && !fits_int32(n.text, negated)) {
    error(pos, "константа " + n.text + " не помещается в int (32 бита)");
  }
  return Ty::of(TypeKind::Int);
}

Ty Checker::call(Call& c, Pos pos) {
  const GlobalSymbol* g = callee(c.callee, pos, false);
  args(g, c.args, pos);
  if (g == nullptr) {
    return {};
  }
  if (g->sig.results.size() != 1) {
    error(pos, callee_noun(*g, false) + " возвращает " +
                   plural(g->sig.results.size(), "значение", "значения", "значений") +
                   "; в выражении допустим только вызов с одним результатом");
    return {};
  }
  return Ty::of(g->sig.results[0]);
}

void Checker::args(const GlobalSymbol* g, std::vector<ExprPtr>& as, Pos pos) {
  if (g != nullptr && as.size() != g->sig.params.size()) {
    error(pos, callee_noun(*g, false) + " ожидает " +
                   plural(g->sig.params.size(), "аргумент", "аргумента", "аргументов") + ", передано " +
                   std::to_string(as.size()));
  }
  for (std::size_t i = 0; i < as.size(); ++i) {
    const Ty t = expr(*as[i]);
    if (g != nullptr && i < g->sig.params.size()) {
      expect(t, g->sig.params[i], as[i]->pos, "аргумент " + std::to_string(i + 1) + " " + callee_noun(*g, true));
    }
  }
}

// a[i] = v хранится как a = set(a, i, v): set(int[], int, int) -> int[].
Ty Checker::array_update(Expr& value) {
  auto& c = std::get<Call>(value.node);
  Expr& array = *c.args[0];
  const std::string& name = std::get<VarRef>(array.node).name;
  const bool ok = expect(expr(array), TypeKind::IntArray, array.pos, "обновление элемента '" + name + "'");
  expect(expr(*c.args[1]), TypeKind::Int, c.args[1]->pos, "индекс массива");
  expect(expr(*c.args[2]), TypeKind::Int, c.args[2]->pos, "значение элемента массива");
  const Ty t = ok ? Ty::of(TypeKind::IntArray) : Ty{};
  annotate(value, t);
  return t;
}

void Checker::cond(Cond& c) {
  std::visit(Overloaded{
                 [&](BoolLit&) {},
                 [&](Compare& cmp) {
                   const std::string what = "сравнение '" + std::string(to_string(cmp.op)) + "'";
                   expect(expr(*cmp.lhs), TypeKind::Int, cmp.lhs->pos, what);
                   expect(expr(*cmp.rhs), TypeKind::Int, cmp.rhs->pos, what);
                 },
                 [&](Not& n) { cond(*n.operand); },
                 [&](Logical& l) {
                   cond(*l.lhs);
                   cond(*l.rhs);
                 },
                 [&](Quantifier& q) {
                   const VarSymbol* prev = find_var(q.var.name);
                   if (prev != nullptr && (ctx_.visible & (1u << static_cast<unsigned>(prev->kind))) != 0) {
                     error(q.var.pos,
                           "повторное объявление '" + q.var.name + "': уже объявлено в " + to_string(prev->pos));
                   }
                   bound_.push_back({q.var.name, VarKind::Bound, q.var.type, q.var.pos, 0});
                   cond(*q.body);
                   bound_.pop_back();
                 },
                 [&](FormulaRef& f) {
                   const GlobalSymbol* g = callee(f.name, c.pos, true);
                   args(g, f.args, c.pos);
                 },
             },
             c.node);
}

}  // namespace funny
