#ifndef FUNNYIR_OPTIMIZE_H
#define FUNNYIR_OPTIMIZE_H

#include "IR.h"

namespace funnyir {

struct OptimizationOptions {
  bool constant_propagation = true;
  bool algebraic_simplification = true;
  bool dead_code_elimination = true;
};

void optimize(IrModule& module, const OptimizationOptions& options = {});

}  // namespace funnyir

#endif  // FUNNYIR_OPTIMIZE_H
