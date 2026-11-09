#ifndef FUNNYSEMA_SYMBOLS_H
#define FUNNYSEMA_SYMBOLS_H

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Ast.h"
#include "Diagnostic.h"

namespace funny {

struct Signature {
  std::vector<TypeKind> params;
  std::vector<TypeKind> results;
};

enum class GlobalKind { Builtin, Function, Formula };

struct GlobalSymbol {
  std::string name;
  GlobalKind kind;
  Signature sig;
  Pos pos;
};

class GlobalScope {
 public:
  GlobalScope(const Module& m, std::vector<Diagnostic>& diags);

  const GlobalSymbol* find(std::string_view name) const;
  const std::vector<GlobalSymbol>& symbols() const { return symbols_; }

 private:
  void add(GlobalSymbol s, std::vector<Diagnostic>& diags);

  std::vector<GlobalSymbol> symbols_;
  std::unordered_map<std::string, std::size_t> index_;
};


std::string to_string(const GlobalSymbol& s);

std::string to_string(const Pos& p);

std::string print_symbols(const Module& m);

}  // namespace funny

#endif  // FUNNYSEMA_SYMBOLS_H
