// Сериализация автомата: JSON и C++-заголовок для лексера проекта.

#ifndef FUNNYLEX_EXPORT_H
#define FUNNYLEX_EXPORT_H

#include <string>

#include "Lexer.h"

namespace funnylex {

std::string export_json(const BuildResult& build);

std::string export_cpp_header(const BuildResult& build, const std::string& ns = "funny_lexer");

std::string escape_string(std::string_view s);

}  // namespace funnylex

#endif  // FUNNYLEX_EXPORT_H
