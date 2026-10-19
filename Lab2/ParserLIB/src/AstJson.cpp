// JSON-вид AST; формат описан в docs/ast.md. Всё дописывается в один буфер,
// поэтому время линейно от размера вывода.

#include <cstdio>
#include <vector>

#include "AstPrinter.h"
#include "Overloaded.h"

namespace funny {

namespace {

std::string quote(std::string_view s) {
  std::string out = "\"";
  for (char c : s) {
    const auto b = static_cast<unsigned char>(c);
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (b < 0x20) {
      char buf[7];
      std::snprintf(buf, sizeof buf, "\\u%04X", b);
      out += buf;
    } else {
      out += c;
    }
  }
  return out + '"';
}

const char* logic_kind(LogicOp op) {
  switch (op) {
    case LogicOp::And: return "And";
    case LogicOp::Or: return "Or";
    case LogicOp::Implies: return "Implies";
  }
  return "?";
}

class JsonWriter {
 public:
  std::string take() { return std::move(out_); }

  void module(const Module& m) {
    open_object();
    key("kind");
    raw(quote("Module"));
    key("decls");
    list(m.decls, [&](const Decl& d) { decl(d); });
    close_object();
    out_ += '\n';
  }

  void expr(const Expr& e) {
    std::visit(Overloaded{
                   [&](const IntLit& n) {
                     node("IntLit", e.pos);
                     field("value", quote(n.text));
                   },
                   [&](const VarRef& v) {
                     node("Var", e.pos);
                     field("name", quote(v.name));
                   },
                   [&](const ArrayAccess& a) {
                     node("ArrayAccess", e.pos);
                     field("array", quote(a.array));
                     key("index");
                     expr(*a.index);
                   },
                   [&](const Call& c) {
                     node("Call", e.pos);
                     field("callee", quote(c.callee));
                     key("args");
                     exprs(c.args);
                   },
                   [&](const Neg& n) {
                     node("Neg", e.pos);
                     key("operand");
                     expr(*n.operand);
                   },
                   [&](const Binary& b) {
                     node("Binary", e.pos);
                     field("op", quote(to_string(b.op)));
                     key("lhs");
                     expr(*b.lhs);
                     key("rhs");
                     expr(*b.rhs);
                   },
               },
               e.node);
    close_object();
  }

 private:
  void cond(const Cond& c) {
    std::visit(Overloaded{
                   [&](const BoolLit& b) {
                     node("BoolLit", c.pos);
                     field("value", b.value ? "true" : "false");
                   },
                   [&](const Compare& cmp) {
                     node("Compare", c.pos);
                     field("op", quote(to_string(cmp.op)));
                     key("lhs");
                     expr(*cmp.lhs);
                     key("rhs");
                     expr(*cmp.rhs);
                   },
                   [&](const Not& n) {
                     node("Not", c.pos);
                     key("operand");
                     cond(*n.operand);
                   },
                   [&](const Logical& l) {
                     node(logic_kind(l.op), c.pos);
                     key("lhs");
                     cond(*l.lhs);
                     key("rhs");
                     cond(*l.rhs);
                   },
                   [&](const Quantifier& q) {
                     node("Quantifier", c.pos);
                     field("quantifier", quote(q.is_forall ? "forall" : "exists"));
                     key("var");
                     var_def(q.var);
                     key("body");
                     cond(*q.body);
                   },
                   [&](const FormulaRef& f) {
                     node("FormulaRef", c.pos);
                     field("name", quote(f.name));
                     key("args");
                     exprs(f.args);
                   },
               },
               c.node);
    close_object();
  }

  void stmt(const Stmt& s) {
    std::visit(Overloaded{
                   [&](const Block& b) {
                     node("Block", s.pos);
                     key("body");
                     list(b.body, [&](const StmtPtr& x) { stmt(*x); });
                   },
                   [&](const If& i) {
                     node("If", s.pos);
                     key("cond");
                     cond(*i.cond);
                     key("then");
                     stmt(*i.then_branch);
                     key("else");
                     opt_stmt(i.else_branch);
                   },
                   [&](const While& w) {
                     node("While", s.pos);
                     key("cond");
                     cond(*w.cond);
                     key("invariant");
                     opt_cond(w.invariant);
                     key("body");
                     stmt(*w.body);
                   },
                   [&](const Assert& a) {
                     node("Assert", s.pos);
                     key("pred");
                     cond(*a.pred);
                   },
                   [&](const Assume& a) {
                     node("Assume", s.pos);
                     key("pred");
                     cond(*a.pred);
                   },
                   [&](const Assign& a) {
                     node("Assign", s.pos);
                     key("target");
                     name(a.target);
                     key("value");
                     expr(*a.value);
                     field("sugar", a.sugar == Sugar::ArrayUpdate ? quote("array_update") : "null");
                   },
                   [&](const TupleAssign& t) {
                     node("TupleAssign", s.pos);
                     key("targets");
                     list(t.targets, [&](const Name& n) { name(n); });
                     key("call");
                     expr(*t.call);
                   },
               },
               s.node);
    close_object();
  }

  void decl(const Decl& d) {
    std::visit(Overloaded{
                   [&](const Function& f) {
                     node("Function", f.name.pos);
                     field("name", quote(f.name.text));
                     key("params");
                     var_defs(f.params);
                     key("requires");
                     opt_cond(f.requires_clause);
                     key("returns");
                     var_defs(f.returns);
                     key("ensures");
                     opt_cond(f.ensures_clause);
                     key("uses");
                     list(f.uses, [&](const LocalDef& l) { local_def(l); });
                     key("body");
                     stmt(*f.body);
                   },
                   [&](const Formula& f) {
                     node("Formula", f.name.pos);
                     field("name", quote(f.name.text));
                     key("params");
                     var_defs(f.params);
                     key("body");
                     cond(*f.body);
                   },
               },
               d);
    close_object();
  }

  void var_def(const VarDef& v) {
    node("VarDef", v.pos);
    field("name", quote(v.name));
    field("type", quote(to_string(v.type)));
    close_object();
  }

  void var_defs(const std::vector<VarDef>& vs) {
    list(vs, [&](const VarDef& v) { var_def(v); });
  }

  void local_def(const LocalDef& l) {
    node("LocalDef", l.pos);
    field("name", quote(l.name));
    field("type", l.type ? quote(to_string(*l.type)) : "null");
    close_object();
  }

  void name(const Name& n) {
    open_object();
    field("name", quote(n.text));
    field("pos", pos_json(n.pos));
    close_object();
  }

  void exprs(const std::vector<ExprPtr>& es) {
    list(es, [&](const ExprPtr& e) { expr(*e); });
  }

  void opt_cond(const CondPtr& c) {
    if (c) {
      cond(*c);
    } else {
      raw("null");
    }
  }

  void opt_stmt(const StmtPtr& s) {
    if (s) {
      stmt(*s);
    } else {
      raw("null");
    }
  }

  // ---- низкий уровень ----

  static std::string pos_json(const Pos& p) {
    return "{\"line\": " + std::to_string(p.line) + ", \"col\": " + std::to_string(p.col) +
           ", \"offset\": " + std::to_string(p.offset) + '}';
  }

  // Открывает объект узла и пишет kind и pos; закрывает его вызывающий.
  void node(std::string_view kind, const Pos& pos) {
    open_object();
    field("kind", quote(kind));
    field("pos", pos_json(pos));
  }

  void open_object() {
    out_ += '{';
    first_.push_back(true);
  }

  void close_object() {
    first_.pop_back();
    out_ += '\n';
    indent();
    out_ += '}';
  }

  template <class T, class F>
  void list(const std::vector<T>& xs, F render) {
    if (xs.empty()) {
      out_ += "[]";
      return;
    }
    out_ += '[';
    first_.push_back(true);
    for (const T& x : xs) {
      next_item();
      render(x);
    }
    first_.pop_back();
    out_ += '\n';
    indent();
    out_ += ']';
  }

  void key(std::string_view k) {
    next_item();
    out_ += quote(k);
    out_ += ": ";
  }

  void field(std::string_view k, const std::string& value) {
    key(k);
    out_ += value;
  }

  void raw(std::string_view s) { out_ += s; }

  // Запятая перед всеми элементами, кроме первого, и перевод строки с отступом.
  void next_item() {
    if (!first_.back()) {
      out_ += ',';
    }
    first_.back() = false;
    out_ += '\n';
    indent();
  }

  void indent() { out_.append(first_.size() * 2, ' '); }

  std::string out_;
  std::vector<bool> first_;  // по одному на открытый объект/массив
};

}  // namespace

std::string print_json(const Expr& e) {
  JsonWriter w;
  w.expr(e);
  return w.take() + '\n';
}

std::string print_json(const Module& m) {
  JsonWriter w;
  w.module(m);
  return w.take();
}

}  // namespace funny
