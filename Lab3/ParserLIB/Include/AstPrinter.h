// Печать AST: дерево с отступами и JSON.

#ifndef FUNNYPARSE_AST_PRINTER_H
#define FUNNYPARSE_AST_PRINTER_H

#include <string>

#include "Ast.h"

namespace funny {

// Строки вида "<Kind> [детали] @L:C", дети с отступом 2 пробела; в конце '\n'.
std::string print_tree(const Expr& e);
std::string print_tree(const Cond& c);
std::string print_tree(const Stmt& s);
// Группы (params, body, cond, ...) — строки без позиции; пустые не печатаются.
std::string print_tree(const Module& m);

// JSON с отступом 2; схема — docs/ast.md.
std::string print_json(const Expr& e);
std::string print_json(const Module& m);

}  // namespace funny

#endif  // FUNNYPARSE_AST_PRINTER_H
