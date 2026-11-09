#ifndef FUNNYIR_TEST_IR_INTERPRETER_H
#define FUNNYIR_TEST_IR_INTERPRETER_H

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "IR.h"

namespace funnyir_test {

using Value = std::variant<std::int32_t, std::vector<std::int32_t>>;
struct ExecutionResult {
  std::vector<Value> results;
  bool trapped = false;
  std::string error;
};

ExecutionResult execute(const funnyir::IrModule& module, std::string_view function, const std::vector<Value>& args);

}  // namespace funnyir_test

#endif  // FUNNYIR_TEST_IR_INTERPRETER_H
