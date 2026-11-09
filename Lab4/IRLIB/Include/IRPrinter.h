#ifndef FUNNYIR_IR_PRINTER_H
#define FUNNYIR_IR_PRINTER_H

#include <string>

#include "IR.h"

namespace funnyir {

std::string print_ir(const IrModule& module);
std::string print_ir_json(const IrModule& module);
std::string print_cfg_dot(const IrFunction& function);

}  // namespace funnyir

#endif  // FUNNYIR_IR_PRINTER_H
