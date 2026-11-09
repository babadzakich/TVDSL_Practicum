#ifndef FUNNYIR_CLI_H
#define FUNNYIR_CLI_H

#include <iosfwd>
#include <string>
#include <vector>

namespace funnyir {

int run_cli(const std::vector<std::string>& args, std::istream& input, std::ostream& output, std::ostream& error);

}  // namespace funnyir

#endif  // FUNNYIR_CLI_H
