#ifndef FUNNYIR_LOWERING_H
#define FUNNYIR_LOWERING_H

#include "Ast.h"
#include "IR.h"

namespace funnyir {

IrModule lower(const funny::Module& module);

}  // namespace funnyir

#endif  // FUNNYIR_LOWERING_H
