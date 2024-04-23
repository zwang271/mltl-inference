#include <boost/container/flat_set.hpp>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <set>
#include <sys/time.h>

// #include "evaluate_mltl.h"
#include "parser.hh"
#include "quine_mccluskey.hh"

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

class NodeWrapper {
public:
  unique_ptr<ASTNode> ast;
  float accuracy;
  int depth;
  NodeWrapper(unique_ptr<ASTNode> ast, float accuracy, int depth)
      : ast(std::move(ast)), accuracy(accuracy), depth(depth) {}

  // determines position in set, break ties with ast lexicographical compare.
  bool operator<(const NodeWrapper &rhs) const {
    if (accuracy != rhs.accuracy) {
      return accuracy < rhs.accuracy;
    }
    size_t lhs_size = ast->size();
    size_t rhs_size = rhs.ast->size();
    if (lhs_size != rhs_size) {
      return lhs_size > rhs_size;
    }
    return *ast > *rhs.ast;
  }
};

struct TestAccuracyCompare {
  bool operator()(const tuple<string, float, float> &lhs,
                  const tuple<string, float, float> &rhs) const {
    if (std::get<2>(lhs) != std::get<2>(rhs)) {
      return std::get<2>(lhs) < std::get<2>(rhs);
    }
    if (std::get<1>(lhs) != std::get<1>(rhs)) {
      return std::get<1>(lhs) < std::get<1>(rhs);
    }
    if (std::get<0>(lhs).length() != std::get<0>(rhs).length()) {
      return std::get<0>(lhs).length() > std::get<0>(rhs).length();
    }
    return std::get<0>(lhs) > std::get<0>(rhs);
  }
};

/*
bool best_first_search(vector<unique_ptr<ASTNode>> &bool_funcs, int *numNodes,
                       function<float(const NodeWrapper &)> h = nullptr) {
  *numNodes = 0;
  set<NodeWrapper> visited;
  set<NodeWrapper> pq;
  // visited.emplace(p);
  // pq.emplace(p);

  while (!pq.empty())>{
    const NodeWrapper &p = *pq.begin();
    pq.erase(p);

    // generate adjacent formulas
    vector<unique_ptr<ASTNode>> adj_list;

    // for (int i = 0; i < num_adj; ++i) {
    //   // p.priority = h(p);
    //   auto reached = visited.find(adj_buf[i]);
    //   if (reached == visited.end()) {
    //     // first time we have seen this node
    //     *numNodes += 1;
    //     adj_buf[i].set_pred(p);
    //     visited.emplace(p);
    //     pq.emplace(adj_buf[i]);
    //   } else if (adj_buf[i].path_cost < reached->path_cost) {
    //     pq.erase(*reached);
    //     adj_buf[i].set_pred(p);
    //     pq.emplace(std::move(new_ast), priority, accuracy);
    // }

      if (p.accuracy == 1) {
        return true;
      }
      if (p.accuracy == 0) {
        cout << p.ast->as_pretty_string();
        return true; // but negate it
      }
    }
  }

  cout << "No solutions exist.\n";
  return false;
}
*/

void replaceVar(ASTNode &ast, unsigned int id, unsigned int new_id) {
  if (ast.get_type() == ASTNode::Type::Variable) {
    Variable &var = static_cast<Variable &>(ast);
    if (var.get_id() == id) {
      var.set_id(new_id);
    }
  } else if (ast.is_unary_op()) {
    ASTNode &operand = static_cast<UnaryOp &>(ast).get_operand();
    replaceVar(operand, id, new_id);
  } else if (ast.is_binary_op()) {
    ASTNode &left = static_cast<BinaryOp &>(ast).get_left();
    replaceVar(left, id, new_id);
    ASTNode &right = static_cast<BinaryOp &>(ast).get_right();
    replaceVar(right, id, new_id);
  }
  return;
}

void generateCombinations(vector<int> &nums, int n, int index,
                          vector<int> &current, vector<vector<int>> &result) {
  if (current.size() == n) {
    result.push_back(current);
    return;
  }

  for (int i = index; i < nums.size(); ++i) {
    current.push_back(nums[i]);
    generateCombinations(nums, n, i + 1, current, result);
    current.pop_back();
  }
}

vector<vector<int>> combinations(vector<int> &nums, int n) {
  vector<vector<int>> result;
  vector<int> current;
  generateCombinations(nums, n, 0, current, result);
  return result;
}

void generate_formulas(vector<unique_ptr<ASTNode>> &formulas, int vars,
                       size_t max_ub, size_t bounds_step) {
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
    for (size_t lb = 0; lb <= max_ub; lb += bounds_step) {
      for (size_t ub = lb + bounds_step; ub <= max_ub; ub += bounds_step) {
        formulas.emplace_back(
            make_unique<Finally>(formulas[i]->deep_copy(), lb, ub));
        formulas.emplace_back(
            make_unique<Globally>(formulas[i]->deep_copy(), lb, ub));
      }
    }
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
    for (size_t lb = 0; lb <= max_ub; lb += bounds_step) {
      for (size_t ub = lb + bounds_step; ub <= max_ub; ub += bounds_step) {
        formulas.emplace_back(make_unique<Until>(
            formulas[i]->deep_copy(), formulas[op]->deep_copy(), lb, ub));
        formulas.emplace_back(make_unique<Release>(
            formulas[i]->deep_copy(), formulas[op]->deep_copy(), lb, ub));
      }
    }
  }

  return;
}

float calc_accuracy(const ASTNode &f, vector<vector<string>> &pos,
                    vector<vector<string>> &neg) {
  int traces_satisified = 0;
  for (size_t j = 0; j < pos.size(); ++j) {
    traces_satisified += f.evaluate(pos[j]);
  }
  for (size_t j = 0; j < neg.size(); ++j) {
    traces_satisified += !f.evaluate(neg[j]);
  }
  return traces_satisified / (float)(pos.size() + neg.size());
};

bool keep_best(boost::container::flat_set<NodeWrapper> &formulas,
               unique_ptr<ASTNode> new_f, float acc, int depth,
               size_t num_to_keep) {
  if (formulas.size() < num_to_keep) {
    formulas.emplace(std::move(new_f), acc, depth);
    return true;
  }
  if (formulas.begin()->accuracy < acc) {
    formulas.erase(formulas.begin());
    formulas.emplace(std::move(new_f), acc, depth);
    return true;
  }
  return false;
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

  // string base_path = "../dataset/nasa-atc_formula1";
  string base_path = "../dataset/nasa-atc_formula2";
  // string base_path = "../dataset/fmsd17_formula1";
  // string base_path = "../dataset/fmsd17_formula2";
  // string base_path = "../dataset/fmsd17_formula3";
  // string base_path = "../dataset/basic_future";
  // string base_path = "../dataset/basic_global";
  // string base_path = "../dataset/basic_release";
  // string base_path = "../dataset/basic_until";
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

  // OPTIONS
  size_t bounds_step = max_pos_train_trace_len / 5;
  size_t max_formulas = 256;
  int max_depth = 3;

  const size_t max_vars = 3;
  const size_t num_vars =
      min((size_t)max_vars, traces_pos_train[0][0].length());
  const size_t num_vars_in_trace = traces_pos_train[0][0].length();
  const size_t truth_table_rows = pow(2, num_vars);
  size_t num_boolean_functions = pow(2, pow(2, num_vars));
  vector<string> inputs(truth_table_rows);
  vector<unique_ptr<ASTNode>> bool_funcs;

  gettimeofday(&start, NULL); // start timer

  if (num_vars == 0) {
    cout << "trace is empty\n";
    exit(-1);
  }
  vector<int> trace_variables;
  for (int i = 0; i < traces_pos_train[0][0].length(); ++i) {
    trace_variables.emplace_back(i);
  }
  vector<vector<int>> input_variables;
  if (traces_pos_train[0][0].length() > num_vars) {
    input_variables = combinations(trace_variables, num_vars);
  } else {
    input_variables.emplace_back(trace_variables);
  }

  // ENUMERATE ALL BOOLEAN FUNCTIONS
  for (uint32_t i = 0; i < truth_table_rows; ++i) {
    inputs[i] = int_to_bin_str(i, num_vars);
  }

  // don't generate the uninteresting cases of always true or always false
  for (uint64_t i = 1; i < num_boolean_functions - 1; ++i) {
    vector<string> implicants;
    for (uint32_t j = 0; j < truth_table_rows; ++j) {
      if ((i >> j) & 1) {
        implicants.emplace_back(inputs[j]);
      }
    }
    bool_funcs.emplace_back(quine_mccluskey(implicants));
  }

  num_boolean_functions = bool_funcs.size();
  for (int i = 1; i < input_variables.size(); ++i) {
    for (size_t j = 0; j < num_boolean_functions; ++j) {
      std::unique_ptr<ASTNode> new_func = bool_funcs[j]->deep_copy();
      for (int k = num_vars - 1; k >= 0; --k) {
        replaceVar(*new_func, k, input_variables[i][k]);
      }
      // only insert if it is not a duplicate
      auto it = find_if(bool_funcs.begin(), bool_funcs.end(),
                        [&new_func](const std::unique_ptr<ASTNode> &ptr) {
                          return *ptr == *new_func;
                        });
      if (it == bool_funcs.end()) {
        bool_funcs.emplace_back(std::move(new_func));
      }
    }
  }
  num_boolean_functions = bool_funcs.size();

  // for (auto &formula : bool_funcs) {
  //   cout << formula->as_pretty_string() << "\n";
  // }
  cout << "num bool funcs: " << num_boolean_functions << "\n";

  size_t max_ub = max_pos_train_trace_len - 1;

  boost::container::flat_set<unique_ptr<ASTNode>, ASTNodeUniquePtrCompare>
      interesting_bool_funcs;

  for (size_t i = 0; i < num_vars_in_trace; ++i) {
    interesting_bool_funcs.emplace(make_unique<Variable>(i));
    interesting_bool_funcs.emplace(
        make_unique<Negation>(make_unique<Variable>(i)));
  }
#pragma omp parallel for schedule(dynamic)
  for (size_t i = 0; i < num_boolean_functions; ++i) {
    if (calc_accuracy(
            *make_unique<Finally>(bool_funcs[i]->deep_copy(), 0, max_ub),
            traces_pos_train, traces_neg_train) > 0.5) {
#pragma omp critical
      interesting_bool_funcs.emplace(bool_funcs[i]->deep_copy());
    }
  }
#pragma omp parallel for schedule(dynamic)
  for (size_t i = 0; i < num_boolean_functions; ++i) {
    if (calc_accuracy(
            *make_unique<Globally>(bool_funcs[i]->deep_copy(), 0, max_ub),
            traces_pos_train, traces_neg_train) > 0.5) {
#pragma omp critical
      interesting_bool_funcs.emplace(bool_funcs[i]->deep_copy());
    }
  }

  for (auto &formula : interesting_bool_funcs) {
    cout << formula->as_pretty_string() << "\n";
  }
  cout << "num interesting bool funcs: " << interesting_bool_funcs.size()
       << "\n";

  boost::container::flat_set<NodeWrapper> formulas;

#pragma omp parallel for schedule(dynamic)
  for (auto &operand1 : interesting_bool_funcs) {
    for (size_t lb = 0; lb <= max_ub; lb += bounds_step) {
      for (size_t ub = lb + bounds_step; ub <= max_ub; ub += bounds_step) {
        unique_ptr<ASTNode> candidate =
            make_unique<Globally>(operand1->deep_copy(), lb, ub);
        float acc =
            calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
        { keep_best(formulas, std::move(candidate), acc, 1, max_formulas); }

        candidate = make_unique<Finally>(operand1->deep_copy(), lb, ub);
        acc = calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
        { keep_best(formulas, std::move(candidate), acc, 1, max_formulas); }
      }
    }
  }

  // remove complex boolean functions now, they take longer to evaluate and make
  // for really complicated formulas as is.
  for (auto itr{interesting_bool_funcs.begin()};
       itr != interesting_bool_funcs.end();) {
    if ((*itr)->size() > 6) {
      itr = interesting_bool_funcs.erase(itr);
    } else {
      ++itr;
    }
  }
  cout << "num reduced interesting bool funcs: "
       << interesting_bool_funcs.size() << "\n";

#pragma omp parallel for schedule(dynamic)
  for (auto &operand1 : interesting_bool_funcs) {
    for (auto &operand2 : interesting_bool_funcs) {
      if (operand1 == operand2) {
        continue;
      }
      for (size_t lb = 0; lb <= max_ub; lb += bounds_step) {
        for (size_t ub = lb + bounds_step; ub <= max_ub; ub += bounds_step) {
          unique_ptr<ASTNode> candidate = make_unique<Until>(
              operand1->deep_copy(), operand2->deep_copy(), lb, ub);
          float acc =
              calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
          { keep_best(formulas, std::move(candidate), acc, 1, max_formulas); }

          candidate = make_unique<Release>(operand1->deep_copy(),
                                           operand2->deep_copy(), lb, ub);
          acc = calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
          { keep_best(formulas, std::move(candidate), acc, 1, max_formulas); }
        }
      }
    }
  }

  for (int depth = 2; depth <= max_depth; ++depth) {
    cout << "GENERATING DEPTH " << depth << " FUNCTIONS\n";
    boost::container::flat_set<NodeWrapper> candidates;
    // (1) GENERATE FORMULAS WITH BOOLEAN ARGUMENT AND DEPTH -1 formulas.
#pragma omp parallel for schedule(dynamic)
    for (auto &operand1 : formulas) {
      unique_ptr<ASTNode> candidate;
      float acc;
      for (size_t lb = 0; lb <= max_ub; lb += bounds_step) {
        for (size_t ub = lb + bounds_step; ub <= max_ub; ub += bounds_step) {
          if (operand1.ast->future_reach() + ub > max_pos_train_trace_len) {
            continue;
          }
          if (operand1.depth == depth - 1) {
            candidate =
                make_unique<Globally>(operand1.ast->deep_copy(), lb, ub);
            acc = calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
            {
              keep_best(candidates, std::move(candidate), acc, depth,
                        max_formulas);
            }

            candidate = make_unique<Finally>(operand1.ast->deep_copy(), lb, ub);
            acc = calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
            {
              keep_best(candidates, std::move(candidate), acc, depth,
                        max_formulas);
            }
          }

          for (auto &operand2 : formulas) {
            if (operand1.ast == operand2.ast) {
              continue;
            }
            if (operand1.depth < depth - 1 && operand2.depth < depth - 1) {
              continue;
            }
            if (operand2.ast->future_reach() + ub > max_pos_train_trace_len) {
              continue;
            }

            candidate = make_unique<Until>(operand1.ast->deep_copy(),
                                           operand2.ast->deep_copy(), lb, ub);
            acc = calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
            {
              keep_best(candidates, std::move(candidate), acc, depth,
                        max_formulas);
            }

            candidate = make_unique<Release>(operand1.ast->deep_copy(),
                                             operand2.ast->deep_copy(), lb, ub);
            acc = calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
            {
              keep_best(candidates, std::move(candidate), acc, depth,
                        max_formulas);
            }
          }

          if (operand1.depth == depth - 1) {
            for (auto &operand2 : interesting_bool_funcs) {
              candidate = make_unique<Until>(operand1.ast->deep_copy(),
                                             operand2->deep_copy(), lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates, std::move(candidate), acc, depth,
                          max_formulas);
              }

              candidate = make_unique<Release>(operand1.ast->deep_copy(),
                                               operand2->deep_copy(), lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates, std::move(candidate), acc, depth,
                          max_formulas);
              }

              candidate = make_unique<Until>(operand2->deep_copy(),
                                             operand1.ast->deep_copy(), lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates, std::move(candidate), acc, depth,
                          max_formulas);
              }

              candidate = make_unique<Release>(
                  operand2->deep_copy(), operand1.ast->deep_copy(), lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates, std::move(candidate), acc, depth,
                          max_formulas);
              }
            }
          }

          // (2) GENERATE FORMULAS WITH AT LEAST ONE DEPTH -1 formula.

          // (integrate and/or?)
        }
      }
    }

    formulas.merge(candidates);
    // cut fat, now get rid of the worst 50% of formulas to avoid growing the
    // state space
    while (formulas.size() > max_formulas) {
      formulas.erase(formulas.begin());
    }
  }

  //   for (auto &operand1 : interesting_bool_funcs) {
  //     if (operand1->size() > 6) {
  //       continue;
  //     }
  //     for (auto &operand2 : interesting_bool_funcs) {
  //       if (operand1 == operand2) {
  //         continue;
  //       }
  //       if (operand2->size() > 6) {
  //         continue;
  //       }
  // #pragma omp parallel for collapse(2)
  //       for (size_t lb = 0; lb <= max_ub; lb += bounds_step) {
  //         for (size_t ub = lb + bounds_step; ub <= max_ub; ub +=
  //         bounds_step)
  //         {
  //           unique_ptr<ASTNode> candidate = make_unique<Release>(
  //               operand1->deep_copy(), operand2->deep_copy(), lb, ub);
  //           float acc =
  //               calc_accuracy(*candidate, traces_pos_train,
  //               traces_neg_train);
  //           if (acc > 0.6) {
  //             // NodeWrapper wrapper(std::move(candidate), acc, 1);
  // #pragma omp critical
  //             { formulas.emplace(std::move(candidate), acc, 1); }
  //           }
  //         }
  //       }
  //     }
  //   }

  // formulas.emplace(
  //     make_unique<Finally>(operand1->deep_copy(), 0, max_ub));
  // formulas.emplace_back(
  //     make_unique<Finally>(bool_funcs[i]->deep_copy(), lb, ub));
  // formulas.emplace_back(
  // make_unique<Globally>(bool_funcs[i]->deep_copy(), lb, ub));
  /*
  #pragma omp parallel for
  for (size_t j = 0; j < num_boolean_functions; ++j) {
    if (i == j) {
      continue;
    }
    // formulas.emplace_back(
    unique_ptr<ASTNode> until = make_unique<Until>(
        bool_funcs[i]->deep_copy(), bool_funcs[j]->deep_copy(), lb, ub);
    float until_accuracy =
        calc_accuracy(*until, traces_pos_test, traces_neg_test);
    if (until_accuracy > 0.80) {
  #pragma omp critical
      formulas.emplace_back(std::move(until));
    }

    // formulas.emplace_back(make_unique<Release>(
    //     bool_funcs[i]->deep_copy(), bool_funcs[j]->deep_copy(), lb,
    //     ub));
    unique_ptr<ASTNode> release = make_unique<Until>(
        bool_funcs[i]->deep_copy(), bool_funcs[j]->deep_copy(), lb, ub);
    float release_accuracy =
        calc_accuracy(*release, traces_pos_test, traces_neg_test);
    if (release_accuracy > 0.80) {
  #pragma omp critical
      formulas.emplace_back(std::move(release));
    }
  }
  */
  // }
  // }
  // }

  /* GenerateFormulas METHOD
  size_t begin_prev_depth = 0;
  size_t bounds_step = max_pos_train_trace_len / 4;
  size_t max_ub = max_pos_train_trace_len - 1;
  vector<unique_ptr<ASTNode>> formulas;

  size_t num_depth_minus_1 = formulas.size();

  size_t max_depth = 4;
  for (int depth = 0; depth <= max_depth; ++depth) {
    cout << "generating formulas of depth " << depth << "\n";
    generate_formulas(formulas, num_vars_in_trace, max_ub, bounds_step);
  }
  */

  gettimeofday(&end, NULL); // stop timer
  time_taken = end.tv_sec + end.tv_usec / 1e6 - start.tv_sec -
               start.tv_usec / 1e6; // in seconds
  //
  // for (auto &formula : formulas) {
  //   cout << formula->as_pretty_string() << "\n";
  // }
  cout << "num bool funcs: " << num_boolean_functions << "\n";
  cout << "num formulas: " << formulas.size() << "\n";
  size_t best_start_idx = formulas.size() - 1 - 10; // top 10
  size_t idx = 0;
  boost::container::flat_set<tuple<string, float, float>, TestAccuracyCompare>
      formulas_by_test_acc;

  cout << "\n\nBEST TRAIN ACCURACY:\n";
  for (const NodeWrapper &wrapper : formulas) {
    float test_acc =
        calc_accuracy(*wrapper.ast, traces_pos_test, traces_neg_test);
    string formula_str = wrapper.ast->as_pretty_string();
    if (idx >= best_start_idx || wrapper.accuracy == 1) {
      cout << formula_str << "\n";
      cout << "  train accuracy: " << wrapper.accuracy << "\n";
      cout << "  test accuracy : " << test_acc << "\n";
    }
    formulas_by_test_acc.emplace(formula_str, wrapper.accuracy, test_acc);
    ++idx;
  }

  idx = 0;
  cout << "\n\nBEST TEST ACCURACY:\n";
  for (const auto &result : formulas_by_test_acc) {
    if (idx >= best_start_idx || std::get<2>(result) == 1) {
      cout << std::get<0>(result) << "\n";
      cout << "  train accuracy: " << std::get<1>(result) << "\n";
      cout << "  test accuracy : " << std::get<2>(result) << "\n";
    }
    ++idx;
  }
  /*
    #pragma omp parallel for num_threads(12)
    for (const auto &wrapper : formulas) {
      // dynamic_cast<MLTLUnaryTempOpNode>(bool_funcs[i])->child;
      int traces_satisified = 0;
      for (size_t j = 0; j < traces_pos_train.size(); ++j) {
        traces_satisified += formulas[i]->evaluate(traces_pos_train[j]);
      }
      for (size_t j = 0; j < traces_neg_train.size(); ++j) {
        traces_satisified += !formulas[i]->evaluate(traces_neg_train[j]);
      }
      float accuracy = traces_satisified /
                       (float)(traces_pos_train.size() +
    traces_neg_train.size());
      // if (accuracy <= 0.40 || accuracy >= 0.60) {
      // if (accuracy >= 0.68) {
      if (accuracy > 0.5) {
    #pragma omp critical
        {
          ++num_accurate;
          cout << formulas[i]->as_pretty_string() << "\n";
          cout << "accuracy: " << accuracy << "\n";
        }
      }
    }*/

  // gettimeofday(&start, NULL); // start timer

  // for (uint64_t i = 0; i < num_boolean_functions; ++i) {
  //   // MLTLNode *new_node = parse(bool_funcs_string[i]);
  //   for (size_t j = 0; j < traces_pos_train.size(); ++j) {
  //     unique_ptr<ASTNode> new_node = parse(bool_funcs_string[i]);
  //     new_node->evaluate(traces_pos_train[j]);
  //     // cout << new_node->as_string() << "\n";
  //   }
  //   for (size_t j = 0; j < traces_neg_train.size(); ++j) {
  //     unique_ptr<ASTNode> new_node = parse(bool_funcs_string[i]);
  //     new_node->evaluate(traces_neg_train[j]);
  //   }
  // }
  // for (uint64_t i = 0; i < num_boolean_functions; ++i) {
  //   unique_ptr<ASTNode> new_node =
  //       parse(bool_funcs_until_string[i]);
  //   for (size_t j = 0; j < traces_pos_train.size(); ++j) {
  //     new_node->evaluate(traces_pos_train[j]);
  //   }
  //   for (size_t j = 0; j < traces_neg_train.size(); ++j) {
  //     new_node->evaluate(traces_neg_train[j]);
  //   }
  // }

  cout << "num_boolean_functions: " << num_boolean_functions << "\n";
  cout << "num formulas: " << formulas.size() << "\n";
  cout << "total time taken: " << time_taken << "s\n";

  // for (int i = 0; i < num_boolean_functions; ++i) {
  //   cout << boolean_functions[i] << "\n";
  // }

  // cout << "total nodes generated: " << numNodes << "\n";
  // cout << "path length: " << path.length() << "\n";
  // cout << "path: " << path << "\n";

  return 0;
}
