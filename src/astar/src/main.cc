#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sys/time.h>

// #include "evaluate_mltl.h"
#include "parser.hh"
#include "quine_mccluskey.hh"
#include <boost/container/flat_set.hpp>

using namespace std;
using namespace libmltl;

size_t max_trace_length(const vector<vector<string>> &traces) {
  size_t max = 0;
  for (auto &trace : traces) {
    max = std::max(max, trace.size());
  }
  return max;
}

struct ASTNodeUniquePtrCompare {
  bool operator()(const std::unique_ptr<ASTNode> &lhs,
                  const std::unique_ptr<ASTNode> &rhs) const {
    return *lhs < *rhs; // Use the ASTNode comparison operators
  }
};

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

  string base_path = "../dataset/nasa-atc_formula2";
  vector<vector<string>> traces_pos_train =
      read_trace_files(base_path + "/pos_train");
  size_t max_pos_train_trace_len = max_trace_length(traces_pos_train);
  vector<vector<string>> traces_neg_train =
      read_trace_files(base_path + "/neg_train");
  size_t max_neg_train_trace_len = max_trace_length(traces_neg_train);
  vector<vector<string>> traces_pos_test =
      read_trace_files(base_path + "/pos_test");
  size_t max_pos_test_trace_len = max_trace_length(traces_pos_test);
  vector<vector<string>> traces_neg_test =
      read_trace_files(base_path + "/neg_test");
  size_t max_neg_test_trace_len = max_trace_length(traces_neg_test);

  struct timeval start, end;
  double time_taken = 0;

  // const size_t num_vars = traces_pos_train[0][0].length();
  const size_t num_vars = min((size_t)4, traces_pos_train[0][0].length());
  const size_t truth_table_rows = pow(2, num_vars);
  const size_t num_boolean_functions = pow(2, pow(2, num_vars));
  vector<string> inputs(truth_table_rows);
  // set<unique_ptr<ASTNode>, ASTNodeUniquePtrCompare> example_set;
  vector<unique_ptr<ASTNode>> boolean_functions_asts;


  gettimeofday(&start, NULL); // start timer

  // ENUMERATE ALL BOOLEAN FUNCTIONS
  for (uint32_t i = 0; i < truth_table_rows; ++i) {
    inputs[i] = int_to_bin_str(i, num_vars);
  }
  for (uint64_t i = 0; i < num_boolean_functions; ++i) {
    vector<string> implicants;
    for (uint32_t j = 0; j < truth_table_rows; ++j) {
      if ((i >> j) & 1) {
        implicants.emplace_back(inputs[j]);
      }
    }
    boolean_functions_asts.emplace_back(quine_mccluskey(implicants));
  }
  gettimeofday(&end, NULL); // stop timer
  time_taken = end.tv_sec + end.tv_usec / 1e6 - start.tv_sec -
               start.tv_usec / 1e6; // in seconds

  for (auto &formula : boolean_functions_asts) {
    cout << formula->as_pretty_string() << "\n";
  }


  // #pragma omp parallel for
  // for (int i = 0; i < num_boolean_functions; ++i) {
  //     string formula = "G[0,3](" + boolean_functions[i] + ")";
  //     bool val =
  //         evaluate_mltl(formula, {"0101", "1101", "0101", "1101"}, false);
  // }

  /*
  #pragma omp parallel for num_threads(1)
    for (uint64_t i = 0; i < num_boolean_functions; ++i) {
      // dynamic_cast<MLTLUnaryTempOpNode>(boolean_functions_asts[i])->child;
      for (int ub = 0; ub < max_pos_train_trace_len; ++ub) {
        for (int lb = 0; lb <= ub; ++lb) {
          if (ub - lb < 8) {
            continue;
          }
          int traces_satisified = 0;
          boolean_functions_asts[i]->set_lower_bound(lb);
          boolean_functions_asts[i]->set_upper_bound(ub);
          for (size_t j = 0; j < traces_pos_train.size(); ++j) {
            traces_satisified +=
                boolean_functions_asts[i]->evaluate(traces_pos_train[j]);
          }
          for (size_t j = 0; j < traces_neg_train.size(); ++j) {
            traces_satisified +=
                !boolean_functions_asts[i]->evaluate(traces_neg_train[j]);
          }

          float accuracy = traces_satisified / (float)(traces_pos_train.size() +
                                                       traces_neg_train.size());
          if (accuracy >= 0) {
            // cout << boolean_functions_asts[i]->as_string() << "\n";
            // cout << "accuracy: " << accuracy << "\n";
          }
        }
      }
    }
  */
  // gettimeofday(&start, NULL); // start timer

  // for (uint64_t i = 0; i < num_boolean_functions; ++i) {
  //   // MLTLNode *new_node = parse(boolean_functions_asts_string[i]);
  //   for (size_t j = 0; j < traces_pos_train.size(); ++j) {
  //     unique_ptr<ASTNode> new_node = parse(boolean_functions_asts_string[i]);
  //     new_node->evaluate(traces_pos_train[j]);
  //     // cout << new_node->as_string() << "\n";
  //   }
  //   for (size_t j = 0; j < traces_neg_train.size(); ++j) {
  //     unique_ptr<ASTNode> new_node = parse(boolean_functions_asts_string[i]);
  //     new_node->evaluate(traces_neg_train[j]);
  //   }
  // }
  // for (uint64_t i = 0; i < num_boolean_functions; ++i) {
  //   unique_ptr<ASTNode> new_node =
  //       parse(boolean_functions_asts_until_string[i]);
  //   for (size_t j = 0; j < traces_pos_train.size(); ++j) {
  //     new_node->evaluate(traces_pos_train[j]);
  //   }
  //   for (size_t j = 0; j < traces_neg_train.size(); ++j) {
  //     new_node->evaluate(traces_neg_train[j]);
  //   }
  // }

  // gettimeofday(&end, NULL); // stop timer
  // time_taken = end.tv_sec + end.tv_usec / 1e6 - start.tv_sec -
  //              start.tv_usec / 1e6; // in seconds
  // cout << "total time taken: " << time_taken << "s\n";
  // gettimeofday(&start, NULL); // start timer
  //
  // for (uint64_t i = 0; i < num_boolean_functions; ++i) {
  //   for (size_t j = 0; j < traces_pos_train.size(); ++j) {
  //     evaluate_mltl(boolean_functions_asts_string[i], traces_pos_train[j]);
  //   }
  //   for (size_t j = 0; j < traces_neg_train.size(); ++j) {
  //     evaluate_mltl(boolean_functions_asts_string[i], traces_neg_train[j]);
  //   }
  // }
  // for (uint64_t i = 0; i < num_boolean_functions; ++i) {
  //   for (size_t j = 0; j < traces_pos_train.size(); ++j) {
  //     evaluate_mltl(boolean_functions_asts_until_string[i],
  //     traces_pos_train[j]);
  //   }
  //   for (size_t j = 0; j < traces_neg_train.size(); ++j) {
  //     evaluate_mltl(boolean_functions_asts_until_string[i],
  //     traces_neg_train[j]);
  //   }
  // }

  cout << "total time taken: " << time_taken << "s\n";

  // for (int i = 0; i < num_boolean_functions; ++i) {
  //   cout << boolean_functions[i] << "\n";
  // }

  // cout << "total nodes generated: " << numNodes << "\n";
  // cout << "path length: " << path.length() << "\n";
  // cout << "path: " << path << "\n";

  return 0;
}
