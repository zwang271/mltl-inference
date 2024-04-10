#include <iostream>

#include "evaluate_mltl.h"


int main(int argc, char *argv[]) {
  // default options
  // TODO

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      // TODO
      // eval_func_p1 = std::stoi(argv[i + 1]);
      // i++;
    } else {
      std::cerr << "error: unknown option " << arg << std::endl;
      return 1;
    }
  }

  bool val = evaluate_mltl("G[0,3]p1", {"01","11","01","11"}, true);
  std::cout << val << "\n";

  return 0;
}
