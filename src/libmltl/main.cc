#include <cmath>
#include <cstdint>
#include <iostream>
#include <sys/time.h>

#include "parser.hh"

using namespace std;

size_t max_trace_length(const vector<vector<string>> &traces) {
  size_t max = 0;
  for (auto &trace : traces) {
    max = std::max(max, trace.size());
  }
  return max;
}

int main(int argc, char *argv[]) {
  // default options
  // TODO

  for (int i = 1; i < argc; ++i) {
    string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      // TODO
      // eval_func_p1 = stoi(argv[i + 1]);
      // i++;
    } else {
      cerr << "error: unknown option " << arg << endl;
      return 1;
    }
  }

  unique_ptr<ASTNode> test = parse("true");
  test = parse("(p0&~p1)R[0,3](p2)");
  cout << test->as_string() << "\n";
  cout << test->evaluate(
              {"001", "001", "101", "000", "000", "101", "100", "110"})
       << "\n";

  return 0;
}
