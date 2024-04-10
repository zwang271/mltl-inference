#include <iostream>
#include <sys/time.h>

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

  struct timeval start, end;
  double time_taken = 0;
  gettimeofday(&start, NULL); // start timer

  bool val = evaluate_mltl("G[0,3]p1", {"01", "11", "01", "11"}, false);
  std::cout << val << "\n";

  gettimeofday(&end, NULL); // stop timer
  time_taken = end.tv_sec + end.tv_usec / 1e6 - start.tv_sec -
               start.tv_usec / 1e6; // in seconds

  // std::cout << "total nodes generated: " << numNodes << "\n";
  std::cout << "total time taken: " << time_taken << "s\n";
  // std::cout << "path length: " << path.length() << "\n";
  // std::cout << "path: " << path << "\n";

  return 0;
}
