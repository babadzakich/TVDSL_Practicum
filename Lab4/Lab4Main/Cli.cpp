#include "Cli.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

#include "AstPrinter.h"
#include "IRPrinter.h"
#include "Lowering.h"
#include "Optimize.h"
#include "RegisterAllocation.h"
#include "Sema.h"
#include "Snapshot.h"

namespace funnyir {
namespace {

constexpr const char* kUsage = R"(Usage:
  funnyir [OPTIONS] [FILE]          compile FILE (or stdin) to IR
Options:
  --json                            print versioned JSON before/after IR and metrics
  --cfg-dot [DIR]                   write per-function CFG DOT files (default: .)
  --interference-dot [DIR]          write per-function interference DOT files (default: .)
  --no-opt                          disable all local optimizations
  --no-constprop                    disable constant propagation
  --no-algebraic                    disable algebraic simplification
  --no-dce                          disable local dead code elimination
  -h, --help                        show this help
)";

struct Options {
  bool json = false;
  bool cfg_dot = false;
  bool interference_dot = false;
  std::string cfg_dir = ".";
  std::string interference_dir = ".";
  std::string input_path;
  OptimizationOptions optimizations;
};

std::optional<std::string> read_input(const std::string& path, std::istream& input) {
  std::ostringstream buffer;
  if (path.empty()) {
    buffer << input.rdbuf();
  } else {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    buffer << file.rdbuf();
  }
  return buffer.str();
}

std::string label_for(const std::string& path) { return path.empty() ? "<stdin>" : path; }

bool starts_with_option(const std::string& value) { return value.size() >= 2 && value[0] == '-' && value[1] == '-'; }

bool is_source_path(const std::string& value) {
  std::error_code ec;
  if (std::filesystem::is_regular_file(value, ec)) return true;
  return std::filesystem::path(value).extension() == ".funny";
}

bool parse_output_option(const std::vector<std::string>& args, std::size_t& index, const std::string& option,
                         bool& enabled, std::string& directory) {
  constexpr std::string_view equal_prefix = "=";
  if (args[index].starts_with(option + std::string(equal_prefix))) {
    enabled = true;
    directory = args[index].substr(option.size() + equal_prefix.size());
    if (directory.empty()) directory = ".";
    return true;
  }
  if (args[index] != option) return false;
  enabled = true;
  directory = ".";
  if (index + 1 < args.size() && !starts_with_option(args[index + 1]) && !is_source_path(args[index + 1])) {
    directory = args[++index];
  }
  return true;
}

std::optional<Options> parse_options(const std::vector<std::string>& args, std::ostream& output, std::ostream& error,
                                     int& exit_code) {
  Options options;
  bool have_input = false;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (arg == "-h" || arg == "--help") {
      output << kUsage;
      exit_code = 0;
      return std::nullopt;
    }
    if (arg == "--json") {
      options.json = true;
    } else if (arg == "--no-opt") {
      options.optimizations = {false, false, false};
    } else if (arg == "--no-constprop") {
      options.optimizations.constant_propagation = false;
    } else if (arg == "--no-algebraic") {
      options.optimizations.algebraic_simplification = false;
    } else if (arg == "--no-dce") {
      options.optimizations.dead_code_elimination = false;
    } else if (parse_output_option(args, i, "--cfg-dot", options.cfg_dot, options.cfg_dir) ||
               parse_output_option(args, i, "--interference-dot", options.interference_dot,
                                   options.interference_dir)) {
      // Output directory is optional; parse_output_option leaves it as "." when omitted.
    } else if (starts_with_option(arg)) {
      error << "неизвестный параметр '" << arg << "'\n" << kUsage;
      exit_code = 2;
      return std::nullopt;
    } else if (!have_input) {
      options.input_path = arg;
      have_input = true;
    } else {
      error << "указано несколько входных файлов\n" << kUsage;
      exit_code = 2;
      return std::nullopt;
    }
  }
  exit_code = 0;
  return options;
}

std::size_t instruction_count(const IrFunction& function) {
  std::size_t count = 0;
  for (const IrBlock& block : function.blocks) count += block.instructions.size();
  return count;
}

std::unordered_set<ValueId> referenced_values(const IrFunction& function) {
  std::unordered_set<ValueId> result;
  for (const IrBlock& block : function.blocks) {
    for (const IrInstruction& instruction : block.instructions) {
      result.insert(instruction.defs.begin(), instruction.defs.end());
      result.insert(instruction.uses.begin(), instruction.uses.end());
    }
    if (const auto* branch = std::get_if<BranchTerm>(&block.terminator)) {
      result.insert(branch->uses.begin(), branch->uses.end());
    } else if (const auto* ret = std::get_if<ReturnTerm>(&block.terminator)) {
      result.insert(ret->values.begin(), ret->values.end());
    }
  }
  return result;
}

std::size_t temporary_count(const IrFunction& function) {
  const auto referenced = referenced_values(function);
  return static_cast<std::size_t>(std::count_if(function.values.begin(), function.values.end(), [&referenced](const IrValue& value) {
    return value.kind == IrValueKind::Temporary && referenced.contains(value.id);
  }));
}

std::size_t value_count(const IrFunction& function) {
  return function.values.size() - (static_cast<std::size_t>(std::count_if(
      function.values.begin(), function.values.end(), [](const IrValue& value) { return value.kind == IrValueKind::Temporary; })) -
      temporary_count(function));
}

struct FunctionMetrics {
  std::string name;
  std::size_t instructions_before = 0;
  std::size_t instructions_after = 0;
  std::size_t values_before = 0;
  std::size_t values_after = 0;
  std::size_t temporaries_before = 0;
  std::size_t temporaries_after = 0;
  std::size_t colors = 0;
};

std::vector<FunctionMetrics> collect_metrics(const IrModule& before, const IrModule& after) {
  std::vector<FunctionMetrics> metrics;
  for (std::size_t i = 0; i < before.functions.size(); ++i) {
    const IrFunction& first = before.functions[i];
    const IrFunction& second = after.functions[i];
    const auto liveness = analyze_liveness(second);
    const auto graph = build_interference_graph(second, liveness);
    const auto coloring = greedy_color(graph);
    metrics.push_back({first.name,
                       instruction_count(first),
                       instruction_count(second),
                       value_count(first),
                       value_count(second),
                       temporary_count(first),
                       temporary_count(second),
                       coloring.color_count});
  }
  return metrics;
}

bool write_graphs(const Options& options, const IrModule& optimized, std::ostream& error) {
  std::error_code ec;
  if (options.cfg_dot) std::filesystem::create_directories(options.cfg_dir, ec);
  if (ec) {
    error << "не удалось создать каталог '" << options.cfg_dir << "': " << ec.message() << '\n';
    return false;
  }
  if (options.interference_dot) std::filesystem::create_directories(options.interference_dir, ec);
  if (ec) {
    error << "не удалось создать каталог '" << options.interference_dir << "': " << ec.message() << '\n';
    return false;
  }

  for (const IrFunction& function : optimized.functions) {
    if (options.cfg_dot) {
      const auto path = std::filesystem::path(options.cfg_dir) / (function.name + ".cfg.dot");
      std::ofstream file(path);
      if (!file) {
        error << "не удалось записать '" << path.string() << "'\n";
        return false;
      }
      file << print_cfg_dot(function);
    }
    if (options.interference_dot) {
      const auto liveness = analyze_liveness(function);
      const auto graph = build_interference_graph(function, liveness);
      const auto coloring = greedy_color(graph);
      const auto path = std::filesystem::path(options.interference_dir) / (function.name + ".interference.dot");
      std::ofstream file(path);
      if (!file) {
        error << "не удалось записать '" << path.string() << "'\n";
        return false;
      }
      file << print_interference_dot(function, graph, coloring);
    }
  }
  return true;
}

std::string metrics_json(const std::vector<FunctionMetrics>& metrics) {
  std::size_t colors = 0;
  std::string out = "{\"functions\":[";
  for (std::size_t i = 0; i < metrics.size(); ++i) {
    if (i != 0) out += ',';
    const FunctionMetrics& item = metrics[i];
    colors = std::max(colors, item.colors);
    out += "{\"name\":\"" + item.name + "\",\"instructions_before\":" + std::to_string(item.instructions_before) +
           ",\"instructions_after\":" + std::to_string(item.instructions_after) +
           ",\"values_before\":" + std::to_string(item.values_before) + ",\"values_after\":" +
           std::to_string(item.values_after) + ",\"temporaries_before\":" + std::to_string(item.temporaries_before) +
           ",\"temporaries_after\":" + std::to_string(item.temporaries_after) + ",\"colors\":" +
           std::to_string(item.colors) + "}";
  }
  return out + "],\"colors\":" + std::to_string(colors) + "}";
}

void print_text_metrics(std::ostream& output, const std::vector<FunctionMetrics>& metrics) {
  std::size_t max_colors = 0;
  output << "Metrics:\n";
  for (const FunctionMetrics& item : metrics) {
    max_colors = std::max(max_colors, item.colors);
    output << "  " << item.name << ": instructions_before=" << item.instructions_before
           << " instructions_after=" << item.instructions_after << " values_before=" << item.values_before
           << " values_after=" << item.values_after << " temporaries_before=" << item.temporaries_before
           << " temporaries_after=" << item.temporaries_after << " colors=" << item.colors << '\n';
  }
  output << "  module_colors=" << max_colors << '\n';
}

}  // namespace

int run_cli(const std::vector<std::string>& args, std::istream& input, std::ostream& output, std::ostream& error) {
  int parse_exit = 0;
  const auto parsed_options = parse_options(args, output, error, parse_exit);
  if (!parsed_options) return parse_exit;
  const Options& options = *parsed_options;

  const std::optional<std::string> source = read_input(options.input_path, input);
  if (!source) {
    error << "не удалось открыть '" << options.input_path << "'\n";
    return 2;
  }
  const funny::AnalyzeResult analyzed = funny::analyze(*source);
  if (!analyzed.ok()) {
    error << funny::render_diagnostics(label_for(options.input_path), analyzed.diagnostics);
    return 1;
  }

  const IrModule before = lower(analyzed.module);
  IrModule after = before;
  optimize(after, options.optimizations);
  const auto metrics = collect_metrics(before, after);
  if (!write_graphs(options, after, error)) return 2;

  if (options.json) {
    output << "{\"schema_version\":1,\"before\":" << print_ir_json(before) << ",\"after\":"
           << print_ir_json(after) << ",\"metrics\":" << metrics_json(metrics) << "}\n";
  } else {
    output << "== IR before optimizations ==\n" << print_ir(before);
    output << "== IR after optimizations ==\n" << print_ir(after);
    print_text_metrics(output, metrics);
  }
  return 0;
}

}  // namespace funnyir
