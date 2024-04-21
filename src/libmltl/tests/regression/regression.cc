#include <cmath>
#include <fstream>
#include <iostream>
#include <sys/time.h>

#include "parser.hh"

using namespace std;
using namespace libmltl;

size_t max_trace_length(const vector<vector<string>> &traces) {
  size_t max = 0;
  for (auto &trace : traces) {
    max = std::max(max, trace.size());
  }
  return max;
}

void generate_formulas(vector<unique_ptr<ASTNode>> &formulas, int vars,
                       size_t max_ub) {
  size_t num_depth_minus_1 = formulas.size();
  if (num_depth_minus_1 == 0) {
    // formulas.push_back(make_unique<Constant>(true));
    // formulas.push_back(make_unique<Constant>(false));
    for (int i = 0; i < vars; ++i) {
      formulas.emplace_back(make_unique<Variable>(i));
    }
    return;
  }

  // create every possible unary operation
  for (size_t i = 0; i < num_depth_minus_1; ++i) {
    if (formulas[i]->get_type() != ASTNode::Type::Negation) {
      // no need to double negate
      formulas.emplace_back(make_unique<Negation>(formulas[i]->deep_copy()));
    }
    for (size_t lb = 0; lb <= max_ub; ++lb) {
      for (size_t ub = lb; ub <= max_ub; ++ub) {
        formulas.emplace_back(
            make_unique<Finally>(formulas[i]->deep_copy(), lb, ub));
        formulas.emplace_back(
            make_unique<Globally>(formulas[i]->deep_copy(), lb, ub));
      }
    }
    formulas.emplace_back(make_unique<Finally>(formulas[i]->deep_copy(), 0, 2));
    formulas.emplace_back(
        make_unique<Globally>(formulas[i]->deep_copy(), 0, 2));
  }

  // create every possible binary operation
  for (size_t i = 0; i < num_depth_minus_1; ++i) {
    size_t op = num_depth_minus_1 - 1 - i;
    formulas.emplace_back(
        make_unique<And>(formulas[i]->deep_copy(), formulas[op]->deep_copy()));
    // formulas.emplace_back(
    //     make_unique<Xor>(formulas[i]->deep_copy(),
    //     formulas[j]->deep_copy()));
    formulas.emplace_back(
        make_unique<Or>(formulas[i]->deep_copy(), formulas[op]->deep_copy()));
    // formulas.emplace_back(make_unique<Implies>(formulas[i]->deep_copy(),
    //                                            formulas[j]->deep_copy()));
    // formulas.emplace_back(make_unique<Equiv>(formulas[i]->deep_copy(),
    //                                          formulas[j]->deep_copy()));
    for (size_t lb = 0; lb <= max_ub; ++lb) {
      for (size_t ub = lb; ub <= max_ub; ++ub) {
        formulas.emplace_back(make_unique<Until>(
            formulas[i]->deep_copy(), formulas[op]->deep_copy(), lb, ub));
        formulas.emplace_back(make_unique<Release>(
            formulas[i]->deep_copy(), formulas[op]->deep_copy(), lb, ub));
      }
    }
    // formulas.emplace_back(make_unique<Until>(formulas[i]->deep_copy(),
    //                                          formulas[op]->deep_copy(), 0,
    //                                          2));
    // formulas.emplace_back(make_unique<Release>(
    //     formulas[i]->deep_copy(), formulas[op]->deep_copy(), 0, 2));
  }

  return;
}

void generate_formulas(vector<unique_ptr<ASTNode>> &formulas, int max_depth,
                       int max_vars, size_t max_ub) {
  for (int depth = 0; depth <= max_depth; ++depth) {
    cout << "generating formulas of depth " << depth << "\n";
    generate_formulas(formulas, max_vars, max_ub);
  }
  return;
}

int main(int argc, char *argv[]) {
  // default options
  int max_vars = 2;
  int max_formula_depth = 3;
  size_t trace_length = 6;
  size_t max_ub = 2;
  string outfilepath = "";
  string reffilepath = "";

  for (int i = 1; i < argc; ++i) {
    string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
    } else if (arg == "-M" || arg == "--max-vars") {
      max_vars = atoi(argv[++i]);
    } else if (arg == "-D" || arg == "--max-depth") {
      max_formula_depth = atoi(argv[++i]);
    } else if (arg == "-ub" || arg == "--max-upper-bound") {
      max_ub = atoi(argv[++i]);
    } else if (arg == "-t" || arg == "--trace-length") {
      trace_length = stoul(argv[++i]);
    } else if (arg == "-o" || arg == "--out-file") {
      outfilepath = argv[++i];
    } else if (arg == "-r" || arg == "--reference-file") {
      reffilepath = argv[++i];
    } else {
      cerr << "error: unknown option " << arg << "\n";
      return -1;
    }
  }

  if (max_vars <= 0) {
    cerr << "error: --max-vars must be positive\n";
    return -1;
  }
  if (max_formula_depth <= 0) {
    cerr << "error: --max-depth must be positive\n";
    return -1;
  }
  if (max_vars * trace_length >= sizeof(size_t) * 8) {
    cerr << "error: overflow, max_vars * trace_length > " << sizeof(size_t) * 8
         << "\n";
    return -1;
  }

  struct timeval start, end;
  double time_taken = 0;

  const size_t num_traces = pow(2, max_vars * trace_length);
  vector<vector<string>> enumerated_traces(
      num_traces, vector<string>(trace_length, string(max_vars, '0')));
  cout << "num possible traces: " << num_traces << "\n";
  gettimeofday(&start, NULL); // start timer
  for (size_t i = 0; i < num_traces; ++i) {
    for (size_t j = 0; j < trace_length; ++j) {
      enumerated_traces[i][j] = int_to_bin_str(i >> (j * max_vars), max_vars);
    }
  }
  gettimeofday(&end, NULL); // stop timer
  time_taken = end.tv_sec + end.tv_usec / 1e6 - start.tv_sec -
               start.tv_usec / 1e6; // in seconds
  cout << "trace generation took: " << time_taken << "s\n";

  vector<unique_ptr<ASTNode>> formulas;
  gettimeofday(&start, NULL); // start timer
  generate_formulas(formulas, max_formula_depth, max_vars, max_ub);
  gettimeofday(&end, NULL); // stop timer
  time_taken = end.tv_sec + end.tv_usec / 1e6 - start.tv_sec -
               start.tv_usec / 1e6; // in seconds
  cout << "num formulas generated: " << formulas.size() << "\n";
  cout << "formula generation took: " << time_taken << "s\n";
  vector<vector<bool>> results(formulas.size(),
                               vector<bool>(num_traces, false));

  gettimeofday(&start, NULL); // start timer
  for (size_t i = 0; i < formulas.size(); ++i) {
    // cout << formulas[i]->as_string() << "\n";
    for (size_t j = 0; j < num_traces; ++j) {
      results[i][j] = formulas[i]->evaluate(enumerated_traces[j]);
    }
  }
  gettimeofday(&end, NULL); // stop timer
  time_taken = end.tv_sec + end.tv_usec / 1e6 - start.tv_sec -
               start.tv_usec / 1e6; // in seconds
  cout << "formula evaluation took: " << time_taken << "s\n";

  if (!outfilepath.empty()) {
    ofstream outfile(outfilepath);
    if (!outfile.is_open()) {
      cout << "Error opening file.\n";
      return -1;
    }
    for (size_t i = 0; i < formulas.size(); ++i) {
      for (size_t j = 0; j < num_traces; ++j) {
        outfile << results[i][j];
      }
      outfile << "\n";
    }
    outfile.close();
    cout << "results saved at: " << outfilepath << "\n";
  }

  if (!reffilepath.empty()) {
    bool valid = true;
    ifstream infile(reffilepath);
    if (!infile.is_open()) {
      cout << "Error opening file.\n";
      return -1;
    }
    std::string line;
    int idx_line = 0;
    while (std::getline(infile, line)) {
      int idx_char = 0;
      for (char c : line) {
        bool ref_result = (c == '1');
        if (results[idx_line][idx_char] != ref_result) {
          valid = false;
          cout << "FAIL: " << formulas[idx_line]->as_string() << "\n      ";
          for (const string &state : enumerated_traces[idx_char]) {
            cout << state << " ";
          }
          cout << "\n";
          break; // only need the first counter example, move on to other
                 // formulas
        }
        idx_char++;
      }
      idx_line++;
    }
    infile.close();
    if (valid) {
      cout << "PASS\n";
    }
  }

  return 0;
}
