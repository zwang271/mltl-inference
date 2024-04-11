#include <cmath>
#include <cstdint>
#include <iostream>
#include <sys/time.h>

#include "evaluate_mltl.h"
#include "quine_mccluskey.h"

typedef struct slice {
  int test;
  int lb;
  int ub;
} slice_t;

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

  struct timeval start, end;
  double time_taken = 0;
  gettimeofday(&start, NULL); // start timer

  // bool val = evaluate_mltl("G[0,3](p1)", {"01", "11", "01", "11"}, false);
  // bool val = evaluate_mltl("F[0,3](p1&p0)", {"01", "11", "01", "11"}, false);
  // bool val = evaluate_mltl("G[0,3](p1&p0)", {"010", "110", "010", "110"}, false);
  bool val = evaluate_mltl("G[0,3](~p0&(~p1&~p2))", {"010", "110", "010", "110"}, false);
  // std::cout << val << "\n";
  // std::vector<std::string> implicants = {"0000", "0001", "0010", "0100",
  //                                        "1000", "0110", "1001", "1011",
  //                                        "1101", "1111"};
  const uint32_t num_vars = 2;
  const uint32_t truth_table_rows = std::pow(2, num_vars);
  const uint64_t num_boolean_functions = std::pow(2, std::pow(2, num_vars));
  std::vector<std::string> inputs(truth_table_rows);
  std::vector<std::string> boolean_functions(num_boolean_functions);

  for (uint32_t i = 0; i < truth_table_rows; ++i) {
    inputs[i] = int_to_bin_str(i, num_vars);
  }

#pragma omp parallel for num_threads(12)
  for (uint64_t i = 0; i < num_boolean_functions; ++i) {
    std::vector<std::string> implicants;
    for (uint32_t j = 0; j < truth_table_rows; ++j) {
      if ((i >> j) & 1) {
        implicants.emplace_back(inputs[j]);
      }
    }
    boolean_functions[i] = quine_mccluskey(&implicants);
  }

  for (int i = 0; i < num_boolean_functions; ++i) {
    std::string formula = "G[0,3](" + boolean_functions[i] + ")";
    std::cout << formula << "\n";
    bool val = evaluate_mltl(formula, {"0101", "1101", "0101", "1101"}, false);
    std::cout << val << "\n";
  }

  gettimeofday(&end, NULL); // stop timer
  time_taken = end.tv_sec + end.tv_usec / 1e6 - start.tv_sec -
               start.tv_usec / 1e6; // in seconds

  // for (int i = 0; i < num_boolean_functions; ++i) {
  //   std::cout << boolean_functions[i] << "\n";
  // }

  // std::cout << "total nodes generated: " << numNodes << "\n";
  std::cout << "total time taken: " << time_taken << "s\n";
  // std::cout << "path length: " << path.length() << "\n";
  // std::cout << "path: " << path << "\n";

  return 0;
}
