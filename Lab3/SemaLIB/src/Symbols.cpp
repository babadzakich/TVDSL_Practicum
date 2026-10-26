#include "Symbols.h"

#include "Overloaded.h"

namespace funny {

namespace {

std::vector<TypeKind> types_of(const std::vector<VarDef>& vs) {
  std::vector<TypeKind> out;
  out.reserve(vs.size());
  for (const VarDef& v : vs) {
    out.push_back(v.type);
  }
  return out;
}

std::string type_list(const std::vector<TypeKind>& ts) {
  std::string out = "(";
  for (std::size_t i = 0; i < ts.size(); ++i) {
    out += (i == 0 ? "" : ", ") + to_string(ts[i]);
  }
  return out + ')';
}

std::string kind_name(GlobalKind k) {
  switch (k) {
    case GlobalKind::Builtin:
      return "builtin";
    case GlobalKind::Function:
      return "function";
    case GlobalKind::Formula:
      return "formula";
  }
  return "?";
}

void var_line(std::string& out, const std::string& name, TypeKind type, std::string_view kind, bool inferred,
              const Pos& pos) {
  out += "  " + name + ": " + to_string(type) + " [" + std::string(kind) + (inferred ? ", inferred" : "") + "] @" +
         to_string(pos) + '\n';
}

}  // namespace

GlobalScope::GlobalScope(const Module& m, std::vector<Diagnostic>& diags) {
  add({"length", GlobalKind::Builtin, {{TypeKind::IntArray}, {TypeKind::Int}}, {0, 0, 0}}, diags);
  for (const Decl& d : m.decls) {
    std::visit(Overloaded{
                   [&](const Function& f) {
                     add({f.name.text, GlobalKind::Function, {types_of(f.params), types_of(f.returns)}, f.name.pos},
                         diags);
                   },
                   [&](const Formula& f) {
                     add({f.name.text, GlobalKind::Formula, {types_of(f.params), {}}, f.name.pos}, diags);
                   },
               },
               d);
  }
}

void GlobalScope::add(GlobalSymbol s, std::vector<Diagnostic>& diags) {
  if (const GlobalSymbol* prev = find(s.name)) {
    diags.push_back({s.pos, "повторное объявление '" + s.name + "': уже объявлено в " + to_string(prev->pos)});
    return;
  }
  index_.emplace(s.name, symbols_.size());
  symbols_.push_back(std::move(s));
}

const GlobalSymbol* GlobalScope::find(std::string_view name) const {
  auto it = index_.find(std::string(name));
  return it == index_.end() ? nullptr : &symbols_[it->second];
}

std::string to_string(const GlobalSymbol& s) {
  std::string results;
  if (s.kind == GlobalKind::Formula) {
    results = "bool";
  } else if (s.sig.results.size() == 1) {
    results = to_string(s.sig.results[0]);
  } else {
    results = type_list(s.sig.results);
  }
  return kind_name(s.kind) + ' ' + type_list(s.sig.params) + " -> " + results;
}

std::string to_string(const Pos& p) {
  return std::to_string(p.line) + ':' + std::to_string(p.col);
}

std::string print_symbols(const Module& m) {
  std::vector<Diagnostic> ignored;
  const GlobalScope globals(m, ignored);
  std::string out = "globals\n";
  for (const GlobalSymbol& s : globals.symbols()) {
    out += "  " + s.name + ": " + to_string(s) + (s.kind == GlobalKind::Builtin ? "" : " @" + to_string(s.pos)) + '\n';
  }
  for (const Decl& d : m.decls) {
    std::visit(Overloaded{
                   [&](const Function& f) {
                     out += "function " + f.name.text + " @" + to_string(f.name.pos) + '\n';
                     for (const VarDef& v : f.params) {
                       var_line(out, v.name, v.type, "param", false, v.pos);
                     }
                     for (const VarDef& v : f.returns) {
                       var_line(out, v.name, v.type, "return", false, v.pos);
                     }
                     for (const LocalDef& l : f.uses) {
                       if (l.type) {
                         var_line(out, l.name, *l.type, "local", l.inferred, l.pos);
                       } else {
                         out += "  " + l.name + ": ? [local] @" + to_string(l.pos) + '\n';
                       }
                     }
                   },
                   [&](const Formula& f) {
                     out += "formula " + f.name.text + " @" + to_string(f.name.pos) + '\n';
                     for (const VarDef& v : f.params) {
                       var_line(out, v.name, v.type, "param", false, v.pos);
                     }
                   },
               },
               d);
  }
  return out;
}

}  // namespace funny
