#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/time.h>

// #include "evaluate_mltl.h"
#include "mltl_ast.h"
#include "quine_mccluskey.h"

using namespace std;

namespace fs = filesystem;

vector<string> read_trace_file(const string &trace_file_path) {
  vector<string> trace;
  ifstream infile;
  string line;
  infile.open(trace_file_path);
  while (getline(infile, line)) {
    char *c = &line[0];
    string tmp;
    while (*c) {
      if (*c == '0' || *c == '1') {
        tmp.push_back(*c);
      }
      ++c;
    }

    trace.push_back(tmp);
  }
  infile.close();
  return trace;
}

vector<vector<string>> read_trace_files(const string &trace_directory_path) {
  vector<vector<string>> traces;
  for (const auto &entry : fs::directory_iterator(trace_directory_path)) {
    vector<string> new_trace = read_trace_file(entry.path());
    traces.emplace_back(new_trace);
  }
  return traces;
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

  string base_path = "../dataset/basic_global";
  vector<vector<string>> traces_pos_train =
      read_trace_files(base_path + "/pos_train");
  vector<vector<string>> traces_neg_train =
      read_trace_files(base_path + "/neg_train");
  vector<vector<string>> traces_pos_test =
      read_trace_files(base_path + "/pos_test");
  vector<vector<string>> traces_neg_test =
      read_trace_files(base_path + "/neg_test");

  struct timeval start, end;
  double time_taken = 0;
  gettimeofday(&start, NULL); // start timer

  const uint32_t num_vars = 3;
  const uint32_t truth_table_rows = pow(2, num_vars);
  const uint64_t num_boolean_functions = pow(2, pow(2, num_vars));
  vector<string> inputs(truth_table_rows);
  vector<unique_ptr<MLTLNode>> boolean_functions_asts(num_boolean_functions);

  for (uint32_t i = 0; i < truth_table_rows; ++i) {
    inputs[i] = int_to_bin_str(i, num_vars);
  }

#pragma omp parallel for num_threads(1)
  for (uint64_t i = 0; i < num_boolean_functions; ++i) {
    vector<string> implicants;
    for (uint32_t j = 0; j < truth_table_rows; ++j) {
      if ((i >> j) & 1) {
        implicants.emplace_back(inputs[j]);
      }
    }
    boolean_functions_asts[i] = make_unique<MLTLUnaryTempOpNode>(
        MLTLUnaryTempOpType::Globally, 0, 10, quine_mccluskey(&implicants));
  }

// #pragma omp parallel for
// for (int i = 0; i < num_boolean_functions; ++i) {
//     string formula = "G[0,3](" + boolean_functions[i] + ")";
//     bool val =
//         evaluate_mltl(formula, {"0101", "1101", "0101", "1101"}, false);
// }

#pragma omp parallel for num_threads(1)
  for (uint64_t i = 0; i < num_boolean_functions; ++i) {
    int traces_satisified = 0;
    for (size_t j = 0; j < traces_pos_train.size(); ++j) {
      traces_satisified += boolean_functions_asts[i]->evaluate(traces_pos_train[j]);
    }
    for (size_t j = 0; j < traces_neg_train.size(); ++j) {
      traces_satisified += !boolean_functions_asts[i]->evaluate(traces_neg_train[j]);
    }

    float accuracy = traces_satisified /
                     (float)(traces_pos_train.size() + traces_neg_train.size());
    if (accuracy >= 0.5) {
      cout << boolean_functions_asts[i]->as_string() << "\n";
      cout << "accuracy: " << accuracy << "\n";
    }
  }

  gettimeofday(&end, NULL); // stop timer
  time_taken = end.tv_sec + end.tv_usec / 1e6 - start.tv_sec -
               start.tv_usec / 1e6; // in seconds

  // for (int i = 0; i < num_boolean_functions; ++i) {
  //   cout << boolean_functions[i] << "\n";
  // }

  // cout << "total nodes generated: " << numNodes << "\n";
  cout << "total time taken: " << time_taken << "s\n";
  // cout << "path length: " << path.length() << "\n";
  // cout << "path: " << path << "\n";

  return 0;
}
