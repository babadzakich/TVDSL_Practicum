// Компактная S-запись AST — только для тестов.

#ifndef FUNNYPARSE_TEST_SEXPR_H
#define FUNNYPARSE_TEST_SEXPR_H

#include <string>

#include "Ast.h"

std::string sexpr(const funny::Expr& e);
std::string sexpr(const funny::Cond& c);
std::string sexpr(const funny::Stmt& s);
std::string sexpr(const funny::Decl& d);

#endif  // FUNNYPARSE_TEST_SEXPR_H
