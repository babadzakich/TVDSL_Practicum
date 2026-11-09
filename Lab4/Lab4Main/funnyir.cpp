#include <iostream>
#include <string>
#include <vector>

#include "Cli.h"

int main(int argc, char** argv) {
  return funnyir::run_cli(std::vector<std::string>(argv + 1, argv + argc), std::cin, std::cout, std::cerr);
}
