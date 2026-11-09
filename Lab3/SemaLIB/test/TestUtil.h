// Общие помощники тестов HW3.

#ifndef FUNNYSEMA_TEST_UTIL_H
#define FUNNYSEMA_TEST_UTIL_H

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include "Sema.h"
#include "Symbols.h"

// Ошибки анализа в виде "L:C: сообщение".
inline std::vector<std::string> errors(std::string_view src) {
  const funny::AnalyzeResult r = funny::analyze(src);
  EXPECT_TRUE(r.parsed) << "синтаксическая ошибка в тесте: " << src;
  std::vector<std::string> out;
  for (const funny::Diagnostic& d : r.diagnostics) {
    out.push_back(funny::to_string(d.pos) + ": " + d.message);
  }
  return out;
}

using Errors = std::vector<std::string>;

#endif  // FUNNYSEMA_TEST_UTIL_H
