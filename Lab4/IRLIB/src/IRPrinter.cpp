#include "IRPrinter.h"

#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

#include "Overloaded.h"

namespace funnyir {
namespace {

std::string type_name(IrType type) { return funny::to_string(type); }

std::string value_name(const IrFunction& function, ValueId id) {
  if (id >= function.values.size()) return "%invalid" + std::to_string(id);
  const IrValue& value = function.values[id];
  if (value.kind == IrValueKind::Temporary) return "%t" + std::to_string(id);
  return "%" + value.name;
}

std::string opcode_name(IrOpcode opcode) {
  switch (opcode) {
    case IrOpcode::Constant: return "const";
    case IrOpcode::Copy: return "copy";
    case IrOpcode::Neg: return "neg";
    case IrOpcode::Add: return "add";
    case IrOpcode::Sub: return "sub";
    case IrOpcode::Mul: return "mul";
    case IrOpcode::Div: return "div";
    case IrOpcode::LoadElement: return "load_element";
    case IrOpcode::ArraySet: return "array_set";
    case IrOpcode::Call: return "call";
    case IrOpcode::Assert: return "assert";
    case IrOpcode::Assume: return "assume";
  }
  return "unknown";
}

std::string operand_text(const IrFunction& function, const IrOperand& operand) {
  return std::visit(
      funny::Overloaded{[&function](ValueRef ref) { return value_name(function, ref.id); },
                        [](std::int64_t value) { return std::to_string(value); }},
      operand);
}

std::string expression_text(const IrFunction& function, const IrPredicateExpr& expr) {
  return std::visit(
      funny::Overloaded{
          [](const IrIntLiteral& node) { return node.text; },
          [](const IrVarRef& node) { return node.name; },
          [&function](const IrArrayAccess& node) {
            return node.array + "[" + expression_text(function, *node.index) + "]";
          },
          [&function](const IrCallExpr& node) {
            std::string out = node.callee + "(";
            for (std::size_t i = 0; i < node.args.size(); ++i) {
              if (i != 0) out += ", ";
              out += expression_text(function, *node.args[i]);
            }
            return out + ")";
          },
          [&function](const IrNegExpr& node) { return "-" + expression_text(function, *node.operand); },
          [&function](const IrBinaryExpr& node) {
            return "(" + expression_text(function, *node.lhs) + " " + std::string(funny::to_string(node.op)) + " " +
                   expression_text(function, *node.rhs) + ")";
          },
          [&function](const IrPredicateValue& node) { return value_name(function, node.id); },
      },
      expr.node);
}

std::string predicate_text(const IrFunction& function, const IrPredicate& predicate) {
  return std::visit(
      funny::Overloaded{
          [](const IrBoolLiteral& node) { return node.value ? std::string("true") : std::string("false"); },
          [&function](const IrCompare& node) {
            return expression_text(function, *node.lhs) + " " + std::string(funny::to_string(node.op)) + " " +
                   expression_text(function, *node.rhs);
          },
          [&function](const IrNot& node) { return "not (" + predicate_text(function, *node.operand) + ")"; },
          [&function](const IrLogical& node) {
            std::string op = "and";
            if (node.op == funny::LogicOp::Or) op = "or";
            if (node.op == funny::LogicOp::Implies) op = "->";
            return "(" + predicate_text(function, *node.lhs) + " " + op + " " +
                   predicate_text(function, *node.rhs) + ")";
          },
          [&function](const IrQuantifier& node) {
            return std::string(node.is_forall ? "forall" : "exists") + " (" + node.name + ": " +
                   type_name(node.type) + " | " + predicate_text(function, *node.body) + ")";
          },
          [&function](const IrFormulaRef& node) {
            std::string out = node.name + "(";
            for (std::size_t i = 0; i < node.args.size(); ++i) {
              if (i != 0) out += ", ";
              out += expression_text(function, *node.args[i]);
            }
            return out + ")";
          },
      },
      predicate.node);
}

std::string module_predicate_text(const IrPredicate& predicate) {
  IrFunction empty;
  return predicate_text(empty, predicate);
}

std::string json_quote(std::string_view text) {
  std::ostringstream out;
  out << '"';
  for (unsigned char ch : text) {
    switch (ch) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (ch < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch) << std::dec;
        } else {
          out << static_cast<char>(ch);
        }
    }
  }
  out << '"';
  return out.str();
}

std::string pos_json(funny::Pos pos) {
  return "{\"line\":" + std::to_string(pos.line) + ",\"col\":" + std::to_string(pos.col) +
         ",\"offset\":" + std::to_string(pos.offset) + "}";
}

std::string ir_kind_name(IrValueKind kind) {
  switch (kind) {
    case IrValueKind::Parameter: return "parameter";
    case IrValueKind::Local: return "local";
    case IrValueKind::Return: return "return";
    case IrValueKind::Temporary: return "temporary";
  }
  return "temporary";
}

std::string operand_json(const IrOperand& operand) {
  return std::visit(funny::Overloaded{[](ValueRef ref) { return "{\"value\":" + std::to_string(ref.id) + "}"; },
                                      [](std::int64_t value) { return "{\"int\":" + std::to_string(value) + "}"; }},
                    operand);
}

std::string expr_json(const IrPredicateExpr& expr) {
  const std::string type = json_quote(type_name(expr.type));
  const std::string pos = pos_json(expr.pos);
  return std::visit(
      funny::Overloaded{
          [&type, &pos](const IrIntLiteral& node) {
            return "{\"kind\":\"int\",\"pos\":" + pos + ",\"value\":" + json_quote(node.text) +
                   ",\"type\":" + type + "}";
          },
          [&type, &pos](const IrVarRef& node) {
            return "{\"kind\":\"var\",\"pos\":" + pos + ",\"name\":" + json_quote(node.name) +
                   ",\"symbol\":" + json_quote(std::string(funny::to_string(node.kind))) + ",\"type\":" + type + "}";
          },
          [&type, &pos](const IrArrayAccess& node) {
            return "{\"kind\":\"array_access\",\"pos\":" + pos + ",\"array\":" + json_quote(node.array) +
                   ",\"index\":" + expr_json(*node.index) + ",\"type\":" + type + "}";
          },
          [&type, &pos](const IrCallExpr& node) {
            std::string out = "{\"kind\":\"call\",\"pos\":" + pos + ",\"callee\":" + json_quote(node.callee) +
                              ",\"args\":[";
            for (std::size_t i = 0; i < node.args.size(); ++i) {
              if (i != 0) out += ',';
              out += expr_json(*node.args[i]);
            }
            return out + "],\"type\":" + type + "}";
          },
          [&type, &pos](const IrNegExpr& node) {
            return "{\"kind\":\"neg\",\"pos\":" + pos + ",\"operand\":" + expr_json(*node.operand) +
                   ",\"type\":" + type + "}";
          },
          [&type, &pos](const IrBinaryExpr& node) {
            return "{\"kind\":\"binary\",\"pos\":" + pos + ",\"op\":" + json_quote(funny::to_string(node.op)) +
                   ",\"lhs\":" + expr_json(*node.lhs) + ",\"rhs\":" + expr_json(*node.rhs) + ",\"type\":" + type + "}";
          },
          [&type, &pos](const IrPredicateValue& node) {
            return "{\"kind\":\"value\",\"pos\":" + pos + ",\"id\":" + std::to_string(node.id) +
                   ",\"type\":" + type + "}";
          },
      },
      expr.node);
}

std::string predicate_json(const IrPredicate& predicate) {
  const std::string pos = pos_json(predicate.pos);
  return std::visit(
      funny::Overloaded{
          [&pos](const IrBoolLiteral& node) {
            return "{\"kind\":\"bool\",\"pos\":" + pos + ",\"value\":" + (node.value ? "true" : "false") + "}";
          },
          [&pos](const IrCompare& node) {
            return "{\"kind\":\"compare\",\"pos\":" + pos + ",\"op\":" + json_quote(funny::to_string(node.op)) +
                   ",\"lhs\":" + expr_json(*node.lhs) + ",\"rhs\":" + expr_json(*node.rhs) + "}";
          },
          [&pos](const IrNot& node) {
            return "{\"kind\":\"not\",\"pos\":" + pos + ",\"operand\":" + predicate_json(*node.operand) + "}";
          },
          [&pos](const IrLogical& node) {
            std::string kind = "and";
            if (node.op == funny::LogicOp::Or) kind = "or";
            if (node.op == funny::LogicOp::Implies) kind = "implies";
            return "{\"kind\":" + json_quote(kind) + ",\"pos\":" + pos + ",\"lhs\":" +
                   predicate_json(*node.lhs) + ",\"rhs\":" + predicate_json(*node.rhs) + "}";
          },
          [&pos](const IrQuantifier& node) {
            return "{\"kind\":\"quantifier\",\"pos\":" + pos + ",\"quantifier\":" +
                   json_quote(node.is_forall ? "forall" : "exists") + ",\"var\":{\"name\":" + json_quote(node.name) +
                   ",\"type\":" + json_quote(type_name(node.type)) + "},\"body\":" + predicate_json(*node.body) + "}";
          },
          [&pos](const IrFormulaRef& node) {
            std::string out = "{\"kind\":\"formula_ref\",\"pos\":" + pos + ",\"name\":" + json_quote(node.name) +
                              ",\"args\":[";
            for (std::size_t i = 0; i < node.args.size(); ++i) {
              if (i != 0) out += ',';
              out += expr_json(*node.args[i]);
            }
            return out + "]}";
          },
      },
      predicate.node);
}

std::string ids_json(const std::vector<ValueId>& ids) {
  std::string out = "[";
  for (std::size_t i = 0; i < ids.size(); ++i) {
    if (i != 0) out += ',';
    out += std::to_string(ids[i]);
  }
  return out + "]";
}

std::string value_ref_json(const IrParameter& value) {
  return "{\"name\":" + json_quote(value.name) + ",\"type\":" + json_quote(type_name(value.type)) +
         ",\"value\":" + std::to_string(value.value) + "}";
}

void print_function(std::ostringstream& out, const IrFunction& function) {
  out << "function " << function.name << '(';
  for (std::size_t i = 0; i < function.params.size(); ++i) {
    if (i != 0) out << ", ";
    out << function.params[i].name << ": " << type_name(function.params[i].type);
  }
  out << ") returns (";
  for (std::size_t i = 0; i < function.returns.size(); ++i) {
    if (i != 0) out << ", ";
    out << function.returns[i].name << ": " << type_name(function.returns[i].type);
  }
  out << ") {\n";
  out << "  values:\n";
  for (const IrValue& value : function.values) {
    out << "    " << value_name(function, value.id) << ": " << type_name(value.type) << " ["
        << ir_kind_name(value.kind) << "]\n";
  }
  if (function.requires_clause) out << "  requires " << predicate_text(function, *function.requires_clause) << "\n";
  if (function.ensures_clause) out << "  ensures " << predicate_text(function, *function.ensures_clause) << "\n";
  for (const IrBlock& block : function.blocks) {
    out << "b" << block.id << ":  ; preds=";
    if (block.predecessors.empty()) out << "[]";
    else {
      out << '[';
      for (std::size_t i = 0; i < block.predecessors.size(); ++i) out << (i ? "," : "") << 'b' << block.predecessors[i];
      out << ']';
    }
    out << " succs=";
    if (block.successors.empty()) out << "[]";
    else {
      out << '[';
      for (std::size_t i = 0; i < block.successors.size(); ++i) out << (i ? "," : "") << 'b' << block.successors[i];
      out << ']';
    }
    out << '\n';
    if (block.loop_invariant) out << "  invariant " << predicate_text(function, *block.loop_invariant) << '\n';
    for (const IrInstruction& instruction : block.instructions) {
      out << "  ";
      for (std::size_t i = 0; i < instruction.defs.size(); ++i) out << (i ? ", " : "") << value_name(function, instruction.defs[i]);
      if (!instruction.defs.empty()) out << " = ";
      out << opcode_name(instruction.opcode);
      if (instruction.opcode == IrOpcode::Call) out << ' ' << instruction.callee;
      if (instruction.predicate) out << ' ' << predicate_text(function, *instruction.predicate);
      for (const IrOperand& operand : instruction.operands) out << ' ' << operand_text(function, operand);
      out << " @" << instruction.pos.line << ':' << instruction.pos.col << '\n';
    }
    std::visit(
        funny::Overloaded{
            [&out](const JumpTerm& term) { out << "  jump b" << term.target << '\n'; },
            [&out, &function](const BranchTerm& term) {
              out << "  branch " << predicate_text(function, term.condition) << ", b" << term.true_target << ", b"
                  << term.false_target << '\n';
            },
            [&out, &function](const ReturnTerm& term) {
              out << "  return";
              for (ValueId value : term.values) out << ' ' << value_name(function, value);
              out << '\n';
            },
        },
        block.terminator);
  }
  out << "}\n";
}

std::string function_json(const IrFunction& function) {
  std::string out = "{\"name\":" + json_quote(function.name) + ",\"entry\":" + std::to_string(function.entry) +
                    ",\"exit\":" + std::to_string(function.exit) + ",\"params\":[";
  for (std::size_t i = 0; i < function.params.size(); ++i) {
    if (i != 0) out += ',';
    out += value_ref_json(function.params[i]);
  }
  out += "],\"returns\":[";
  for (std::size_t i = 0; i < function.returns.size(); ++i) {
    if (i != 0) out += ',';
    out += value_ref_json(function.returns[i]);
  }
  out += "],\"requires\":" + (function.requires_clause ? predicate_json(*function.requires_clause) : "null") +
         ",\"ensures\":" + (function.ensures_clause ? predicate_json(*function.ensures_clause) : "null") + ",\"values\":[";
  for (std::size_t i = 0; i < function.values.size(); ++i) {
    if (i != 0) out += ',';
    const IrValue& value = function.values[i];
    out += "{\"id\":" + std::to_string(value.id) + ",\"name\":" + json_quote(value.name) +
           ",\"kind\":" + json_quote(ir_kind_name(value.kind)) + ",\"type\":" + json_quote(type_name(value.type)) + "}";
  }
  out += "],\"blocks\":[";
  for (std::size_t i = 0; i < function.blocks.size(); ++i) {
    if (i != 0) out += ',';
    const IrBlock& block = function.blocks[i];
    out += "{\"id\":" + std::to_string(block.id) + ",\"instructions\":[";
    for (std::size_t j = 0; j < block.instructions.size(); ++j) {
      if (j != 0) out += ',';
      const IrInstruction& instruction = block.instructions[j];
      out += "{\"op\":" + json_quote(opcode_name(instruction.opcode)) + ",\"pos\":" + pos_json(instruction.pos) +
             ",\"defs\":" + ids_json(instruction.defs) + ",\"uses\":" + ids_json(instruction.uses) + ",\"operands\":[";
      for (std::size_t k = 0; k < instruction.operands.size(); ++k) {
        if (k != 0) out += ',';
        out += operand_json(instruction.operands[k]);
      }
      out += "],\"callee\":" + (instruction.callee.empty() ? "null" : json_quote(instruction.callee)) +
             ",\"predicate\":" + (instruction.predicate ? predicate_json(*instruction.predicate) : "null") + "}";
    }
    out += "],\"terminator\":";
    out += std::visit(
        funny::Overloaded{
            [](const JumpTerm& term) { return "{\"kind\":\"jump\",\"target\":" + std::to_string(term.target) + "}"; },
            [](const BranchTerm& term) {
              return "{\"kind\":\"branch\",\"condition\":" + predicate_json(term.condition) +
                     ",\"true\":" + std::to_string(term.true_target) + ",\"false\":" +
                     std::to_string(term.false_target) + ",\"uses\":" + ids_json(term.uses) + "}";
            },
            [](const ReturnTerm& term) { return "{\"kind\":\"return\",\"values\":" + ids_json(term.values) + "}"; },
        },
        block.terminator);
    out += ",\"successors\":" + ids_json(block.successors) + ",\"predecessors\":" + ids_json(block.predecessors) +
           ",\"loop_invariant\":" + (block.loop_invariant ? predicate_json(*block.loop_invariant) : "null") + "}";
  }
  return out + "]}";
}

std::string formula_json(const IrFormula& formula) {
  std::string out = "{\"name\":" + json_quote(formula.name) + ",\"params\":[";
  for (std::size_t i = 0; i < formula.params.size(); ++i) {
    if (i != 0) out += ',';
    out += "{\"name\":" + json_quote(formula.params[i].name) + ",\"type\":" + json_quote(type_name(formula.params[i].type)) + "}";
  }
  return out + "],\"body\":" + predicate_json(formula.body) + "}";
}

}  // namespace

std::string print_ir(const IrModule& module) {
  std::ostringstream out;
  std::size_t function_index = 0;
  std::size_t formula_index = 0;
  for (const IrDeclRef& decl : module.declaration_order) {
    if (decl.kind == IrDeclKind::Function && decl.index < module.functions.size()) {
      print_function(out, module.functions[decl.index]);
      ++function_index;
    } else if (decl.kind == IrDeclKind::Formula && decl.index < module.formulas.size()) {
      const IrFormula& formula = module.formulas[decl.index];
      out << "formula " << formula.name << "(";
      for (std::size_t i = 0; i < formula.params.size(); ++i) {
        if (i != 0) out << ", ";
        out << formula.params[i].name << ": " << type_name(formula.params[i].type);
      }
      out << ") => " << module_predicate_text(formula.body) << "\n";
      ++formula_index;
    }
  }
  if (module.declaration_order.empty()) {
    for (const IrFormula& formula : module.formulas) out << "formula " << formula.name << "\n";
    for (const IrFunction& function : module.functions) print_function(out, function);
  } else {
    for (; formula_index < module.formulas.size(); ++formula_index) out << "formula " << module.formulas[formula_index].name << "\n";
    for (; function_index < module.functions.size(); ++function_index) print_function(out, module.functions[function_index]);
  }
  return out.str();
}

std::string print_ir_json(const IrModule& module) {
  std::string out = "{\"schema_version\":1,\"declaration_order\":[";
  for (std::size_t i = 0; i < module.declaration_order.size(); ++i) {
    if (i != 0) out += ',';
    const IrDeclRef& decl = module.declaration_order[i];
    out += "{\"kind\":" + json_quote(decl.kind == IrDeclKind::Function ? "function" : "formula") +
           ",\"index\":" + std::to_string(decl.index) + "}";
  }
  out += "],\"formulas\":[";
  for (std::size_t i = 0; i < module.formulas.size(); ++i) {
    if (i != 0) out += ',';
    out += formula_json(module.formulas[i]);
  }
  out += "],\"functions\":[";
  for (std::size_t i = 0; i < module.functions.size(); ++i) {
    if (i != 0) out += ',';
    out += function_json(module.functions[i]);
  }
  return out + "]}\n";
}

std::string print_cfg_dot(const IrFunction& function) {
  std::ostringstream out;
  out << "digraph " << json_quote(function.name) << " {\n  node [shape=box];\n";
  for (const IrBlock& block : function.blocks) out << "  b" << block.id << " [label=\"b" << block.id << "\"];\n";
  for (const IrBlock& block : function.blocks) {
    for (BlockId successor : block.successors) out << "  b" << block.id << " -> b" << successor << ";\n";
  }
  out << "}\n";
  return out.str();
}

}  // namespace funnyir
