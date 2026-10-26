// Области видимости, типы, объявления и операторы.

#include "Checker.h"

#include "Overloaded.h"

namespace funny {

namespace {

constexpr unsigned bit(VarKind k) {
  return 1u << static_cast<unsigned>(k);
}

constexpr unsigned kAllVars = bit(VarKind::Param) | bit(VarKind::Return) | bit(VarKind::Local) | bit(VarKind::Bound);

}  // namespace

// ---- области видимости ----

void Checker::begin_scope() {
  vars_.clear();
  bound_.clear();
  init_.clear();
}

void Checker::declare(const std::string& name, VarKind kind, std::optional<TypeKind> type, Pos pos) {
  if (const VarSymbol* prev = find_var(name)) {
    error(pos, "повторное объявление '" + name + "': уже объявлено в " + to_string(prev->pos));
    return;
  }
  vars_.push_back({name, kind, type, pos, vars_.size(), !type.has_value()});
  init_.push_back(kind == VarKind::Param);
}

const VarSymbol* Checker::find_var(std::string_view name) const {
  for (auto it = bound_.rbegin(); it != bound_.rend(); ++it) {
    if (it->name == name) {
      return &*it;
    }
  }
  for (const VarSymbol& v : vars_) {
    if (v.name == name) {
      return &v;
    }
  }
  return nullptr;
}

const VarSymbol* Checker::lookup(const std::string& name, Pos pos) {
  const VarSymbol* s = find_var(name);
  if (s == nullptr) {
    const GlobalSymbol* g = globals_.find(name);
    if (g == nullptr) {
      error(pos, "неизвестное имя '" + name + "'");
    } else {
      error(pos, "'" + name + "' — " + (g->kind == GlobalKind::Formula ? "формула" : "функция") + ", а не переменная");
    }
    return nullptr;
  }
  if ((ctx_.visible & bit(s->kind)) == 0) {
    error(pos,
          "'" + name + "' недоступно в " + std::string(ctx_.where) + ": там видны только " + std::string(ctx_.hint));
    return nullptr;
  }
  return s;
}

const VarSymbol* Checker::assign_target(const Name& target) {
  const VarSymbol* s = lookup(target.text, target.pos);
  if (s != nullptr && s->kind == VarKind::Param) {
    error(target.pos, "параметр '" + target.text + "' доступен только для чтения");
    return nullptr;
  }
  return s;
}

const GlobalSymbol* Checker::callee(const std::string& name, Pos pos, bool want_formula) {
  const GlobalSymbol* g = globals_.find(name);
  if (g == nullptr) {
    error(pos, (want_formula ? "неизвестная формула '" : "неизвестная функция '") + name + "'");
    return nullptr;
  }
  const bool is_formula = g->kind == GlobalKind::Formula;
  if (is_formula && !want_formula) {
    error(pos, "'" + name + "' — формула; формулы используются только в предикатах");
    return nullptr;
  }
  if (!is_formula && want_formula) {
    error(pos, "'" + name + "' — функция, а не формула");
    return nullptr;
  }
  return g;
}

// ---- типы ----

// Локальная без аннотации отдаёт ссылку на свою строку в vars_: её тип может появиться позже.
Ty Checker::type_of(const VarSymbol& s) const {
  return s.inferred ? Ty::variable(static_cast<int>(s.id)) : Ty::of(*s.type);
}

std::optional<TypeKind> Checker::resolve(Ty t) const {
  switch (t.kind) {
    case Ty::Kind::Int:
      return TypeKind::Int;
    case Ty::Kind::IntArray:
      return TypeKind::IntArray;
    case Ty::Kind::Var:
      return vars_[t.var].type;
    case Ty::Kind::Error:
      return std::nullopt;
  }
  return std::nullopt;
}

// Вывод типа: записать тип локальной без аннотации в её строку таблицы.
void Checker::infer(int var, TypeKind t, Pos at) {
  vars_[var].type = t;
  vars_[var].inferred_at = at;
}

std::string Checker::describe(Ty t) const {
  const std::optional<TypeKind> r = resolve(t);
  if (!r) {
    return "?";
  }
  std::string out = to_string(*r);
  if (t.kind == Ty::Kind::Var) {
    const VarSymbol& s = vars_[t.var];
    out += " (тип '" + s.name + "' выведен в " + to_string(s.inferred_at) + ")";
  }
  return out;
}

bool Checker::expect(Ty got, TypeKind want, Pos at, const std::string& what) {
  if (got.kind == Ty::Kind::Error) {
    return false;
  }
  const std::optional<TypeKind> r = resolve(got);
  if (!r) {
    infer(got.var, want, at);
    return true;
  }
  if (*r != want) {
    error(at, what + ": ожидается " + to_string(want) + ", получено " + describe(got));
    return false;
  }
  return true;
}

void Checker::assign_types(const VarSymbol& target, Ty value, Pos at) {
  if (value.kind == Ty::Kind::Error) {
    return;
  }
  const Ty target_type = type_of(target);
  const std::optional<TypeKind> tt = resolve(target_type);
  const std::optional<TypeKind> vt = resolve(value);
  if (tt && vt) {
    if (*tt != *vt) {
      error(at, "присваивание '" + target.name + "': переменная имеет тип " + describe(target_type) + ", значение — " +
                    describe(value));
    }
  } else if (vt) {
    infer(static_cast<int>(target.id), *vt, at);
  } else if (tt) {
    infer(value.var, *tt, at);
  }
  
}

void Checker::annotate(Expr& e, Ty t) const {
  const std::optional<TypeKind> r = resolve(t);
  e.type = r ? ExprType{*r} : ExprType{};
}

// ---- объявления ----

void Checker::function(Function& f) {
  begin_scope();
  for (const VarDef& v : f.params) {
    declare(v.name, VarKind::Param, v.type, v.pos);
  }
  for (const VarDef& v : f.returns) {
    declare(v.name, VarKind::Return, v.type, v.pos);
  }
  std::vector<std::pair<LocalDef*, std::size_t>> untyped;  // локальная и её строка в vars_
  for (LocalDef& l : f.uses) {
    const std::size_t id = vars_.size();
    declare(l.name, VarKind::Local, l.type, l.pos);
    if (!l.type && vars_.size() != id) {
      untyped.emplace_back(&l, id);
    }
  }

  if (f.requires_clause) {
    with_context({bit(VarKind::Param) | bit(VarKind::Bound), "requires", "параметры", true, false}, *f.requires_clause);
  }
  if (f.ensures_clause) {
    with_context({bit(VarKind::Param) | bit(VarKind::Return) | bit(VarKind::Bound), "ensures",
                  "параметры и возвращаемые значения", true, false},
                 *f.ensures_clause);
  }
  ctx_ = {kAllVars, "", "", false, true};
  stmt(*f.body);

  for (auto [l, id] : untyped) {
    if (const std::optional<TypeKind> t = vars_[id].type) {
      l->type = *t;
      l->inferred = true;
    } else {
      error(l->pos, "не удалось вывести тип локальной переменной '" + l->name + "': укажите тип явно");
    }
  }
}

void Checker::formula(Formula& f) {
  begin_scope();
  for (const VarDef& v : f.params) {
    declare(v.name, VarKind::Param, v.type, v.pos);
  }
  with_context({bit(VarKind::Param) | bit(VarKind::Bound), "формуле", "параметры", true, false}, *f.body);
}

void Checker::with_context(Context ctx, Cond& c) {
  const Context saved = ctx_;
  ctx_ = ctx;
  cond(c);
  ctx_ = saved;
}

// ---- операторы ----

void Checker::stmt(Stmt& s) {
  std::visit(Overloaded{
                 [&](Block& b) {
                   for (StmtPtr& child : b.body) {
                     stmt(*child);
                   }
                 },
                 [&](If& i) {
                   cond(*i.cond);
                   const std::vector<bool> before = init_;
                   stmt(*i.then_branch);
                   std::vector<bool> after_then = std::move(init_);
                   init_ = before;
                   if (i.else_branch) {
                     stmt(*i.else_branch);
                   }
                   for (std::size_t k = 0; k < init_.size(); ++k) {
                     init_[k] = init_[k] && after_then[k];
                   }
                 },
                 [&](While& w) {
                   cond(*w.cond);
                   if (w.invariant) {
                     with_context({ctx_.visible, ctx_.where, ctx_.hint, true, true}, *w.invariant);
                   }
                   const std::vector<bool> before = init_;
                   stmt(*w.body);
                   init_ = before;
                 },
                 [&](Assert& a) { with_context({ctx_.visible, ctx_.where, ctx_.hint, true, true}, *a.pred); },
                 [&](Assume& a) { with_context({ctx_.visible, ctx_.where, ctx_.hint, true, true}, *a.pred); },
                 [&](Assign& a) { assign(a); },
                 [&](TupleAssign& t) { tuple_assign(t); },
             },
             s.node);
}

void Checker::assign(Assign& a) {
  const Ty value = a.sugar == Sugar::ArrayUpdate ? array_update(*a.value) : expr(*a.value);
  if (const VarSymbol* s = assign_target(a.target)) {
    assign_types(*s, value, a.value->pos);
    init_[s->id] = true;
  }
}

void Checker::tuple_assign(TupleAssign& t) {
  auto& c = std::get<Call>(t.call->node);
  const GlobalSymbol* g = callee(c.callee, t.call->pos, false);
  args(g, c.args, t.call->pos);
  const std::vector<TypeKind>* results = g != nullptr ? &g->sig.results : nullptr;
  if (results != nullptr) {
    t.call->type = *results;
    if (results->size() != t.targets.size()) {
      error(t.call->pos, "функция '" + c.callee + "' возвращает " +
                             plural(results->size(), "значение", "значения", "значений") + ", а в левой части " +
                             plural(t.targets.size(), "переменная", "переменные", "переменных"));
    }
  }
  for (std::size_t i = 0; i < t.targets.size(); ++i) {
    const Name& target = t.targets[i];
    bool repeated = false;
    for (std::size_t j = 0; j < i; ++j) {
      repeated = repeated || t.targets[j].text == target.text;
    }
    if (repeated) {
      error(target.pos, "переменная '" + target.text + "' повторяется в левой части");
      continue;
    }
    if (const VarSymbol* s = assign_target(target)) {
      if (results != nullptr && i < results->size()) {
        assign_types(*s, Ty::of((*results)[i]), target.pos);
      }
      init_[s->id] = true;
    }
  }
}

}  // namespace funny
