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

struct ASTNodeSharedPtrCompare {
  bool operator()(const std::shared_ptr<ASTNode> &lhs,
                  const std::shared_ptr<ASTNode> &rhs) const {
    return *lhs < *rhs; // Use the ASTNode comparison operators
  }
};

class NodeWrapper {
public:
  shared_ptr<ASTNode> ast;
  float accuracy;
  int depth;
  NodeWrapper(shared_ptr<ASTNode> ast, float accuracy, int depth)
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
  bool operator>(const NodeWrapper &rhs) const { return rhs < *this; }
};

bool TestAccLesser(const tuple<string, float, float> &lhs,
                   const tuple<string, float, float> &rhs) {
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

struct TestAccuracyCompare {
  bool operator()(const tuple<string, float, float> &lhs,
                  const tuple<string, float, float> &rhs) const {
    return TestAccLesser(lhs, rhs);
  }
};

struct WorstTestAccuracyCompare {
  bool operator()(const tuple<string, float, float> &lhs,
                  const tuple<string, float, float> &rhs) const {
    return TestAccLesser(rhs, lhs);
  }
};

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

void generate_formulas(vector<shared_ptr<ASTNode>> &formulas, int vars,
                       size_t max_ub, size_t bounds_step) {
  size_t num_depth_minus_1 = formulas.size();
  if (num_depth_minus_1 == 0) {
    // formulas.push_back(make_shared<Constant>(true));
    // formulas.push_back(make_shared<Constant>(false));
    for (int i = 0; i < vars; ++i) {
      formulas.emplace_back(make_shared<Variable>(i));
    }
    return;
  }

  // create every possible unary operation
  for (size_t i = 0; i < num_depth_minus_1; ++i) {
    if (formulas[i]->get_type() != ASTNode::Type::Negation) {
      // no need to double negate
      formulas.emplace_back(make_shared<Negation>(formulas[i]));
    }
    for (size_t lb = 0; lb <= max_ub; lb += bounds_step) {
      for (size_t ub = lb + bounds_step; ub <= max_ub; ub += bounds_step) {
        formulas.emplace_back(make_shared<Finally>(formulas[i], lb, ub));
        formulas.emplace_back(make_shared<Globally>(formulas[i], lb, ub));
      }
    }
  }

  // create every possible binary operation
  for (size_t i = 0; i < num_depth_minus_1; ++i) {
    size_t op = num_depth_minus_1 - 1 - i;
    formulas.emplace_back(make_shared<And>(formulas[i], formulas[op]));
    // formulas.emplace_back(
    //     make_shared<Xor>(formulas[i],
    //     formulas[j]));
    formulas.emplace_back(make_shared<Or>(formulas[i], formulas[op]));
    // formulas.emplace_back(make_shared<Implies>(formulas[i],
    //                                            formulas[j]));
    // formulas.emplace_back(make_shared<Equiv>(formulas[i],
    //                                          formulas[j]));
    for (size_t lb = 0; lb <= max_ub; lb += bounds_step) {
      for (size_t ub = lb + bounds_step; ub <= max_ub; ub += bounds_step) {
        formulas.emplace_back(
            make_shared<Until>(formulas[i], formulas[op], lb, ub));
        formulas.emplace_back(
            make_shared<Release>(formulas[i], formulas[op], lb, ub));
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

bool keep_best(
    boost::container::flat_set<NodeWrapper> &formulas_best,
    boost::container::flat_set<NodeWrapper, std::greater<NodeWrapper>>
        &formulas_worst,
    shared_ptr<ASTNode> new_f, float acc, int depth, size_t num_to_keep) {
  bool inserted = false;
  if (formulas_worst.size() < num_to_keep) {
    formulas_worst.emplace(new_f, acc, depth);
    formulas_best.emplace(std::move(new_f), acc, depth);
    return true;
  }
  if (formulas_worst.begin()->accuracy > acc) {
    formulas_worst.erase(formulas_worst.begin());
    formulas_worst.emplace(new_f, acc, depth);
    inserted = true;
  }
  if (formulas_best.begin()->accuracy < acc) {
    formulas_best.erase(formulas_best.begin());
    formulas_best.emplace(std::move(new_f), acc, depth);
    inserted = true;
  }
  return inserted;
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
  // string base_path = "../dataset/nasa-atc_formula2";
  // string base_path = "../dataset/rv14_formula1";
  string base_path = "../dataset/rv14_formula2";
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
  // size_t bounds_step = max_pos_train_trace_len / 5;
  size_t bounds_step = 5;
  size_t max_formulas = 256;
  int max_depth = 2;
  size_t max_vars = 3; // per sub boolean function
  size_t max_bool_func_size = 6;

  const size_t num_vars =
      min((size_t)max_vars, traces_pos_train[0][0].length());
  const size_t num_vars_in_trace = traces_pos_train[0][0].length();
  const size_t truth_table_rows = pow(2, num_vars);
  size_t num_boolean_functions = pow(2, pow(2, num_vars));
  vector<string> inputs(truth_table_rows);
  vector<shared_ptr<ASTNode>> bool_funcs;

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
      std::shared_ptr<ASTNode> new_func = bool_funcs[j]->deep_copy();
      for (int k = num_vars - 1; k >= 0; --k) {
        replaceVar(*new_func, k, input_variables[i][k]);
      }
      // only insert if it is not a duplicate
      auto it = find_if(bool_funcs.begin(), bool_funcs.end(),
                        [&new_func](const std::shared_ptr<ASTNode> &ptr) {
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
  cout << "considering bool funcs: " << num_boolean_functions << "\n";

  size_t max_ub = max_pos_train_trace_len - 1;

  boost::container::flat_set<shared_ptr<ASTNode>, ASTNodeSharedPtrCompare>
      interesting_bool_funcs;

  for (size_t i = 0; i < num_vars_in_trace; ++i) {
    interesting_bool_funcs.emplace(make_shared<Variable>(i));
    interesting_bool_funcs.emplace(
        make_shared<Negation>(make_shared<Variable>(i)));
  }
#pragma omp parallel for schedule(dynamic)
  for (size_t i = 0; i < num_boolean_functions; ++i) {
    if (calc_accuracy(*make_shared<Finally>(bool_funcs[i], 0, max_ub),
                      traces_pos_train, traces_neg_train) > 0.5) {
#pragma omp critical
      interesting_bool_funcs.emplace(bool_funcs[i]);
    }
  }
#pragma omp parallel for schedule(dynamic)
  for (size_t i = 0; i < num_boolean_functions; ++i) {
    if (calc_accuracy(*make_shared<Globally>(bool_funcs[i], 0, max_ub),
                      traces_pos_train, traces_neg_train) > 0.5) {
#pragma omp critical
      interesting_bool_funcs.emplace(bool_funcs[i]);
    }
  }

  for (auto &formula : interesting_bool_funcs) {
    cout << formula->as_pretty_string() << "\n";
  }
  cout << "interesting bool funcs: " << interesting_bool_funcs.size() << "\n";

  boost::container::flat_set<NodeWrapper> formulas_best;
  boost::container::flat_set<NodeWrapper, std::greater<NodeWrapper>>
      formulas_worst;

  cout << "GENERATING DEPTH 1 FUNCTIONS\n";
#pragma omp parallel for schedule(dynamic)
  for (auto &operand1 : interesting_bool_funcs) {
    for (size_t lb = 0; lb <= max_ub; lb += bounds_step) {
      for (size_t ub = lb + bounds_step; ub <= max_ub; ub += bounds_step) {
        shared_ptr<ASTNode> candidate = make_shared<Globally>(operand1, lb, ub);
        float acc =
            calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
        {
          keep_best(formulas_best, formulas_worst, std::move(candidate), acc, 1,
                    max_formulas);
        }

        candidate = make_shared<Finally>(operand1, lb, ub);
        acc = calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
        {
          keep_best(formulas_best, formulas_worst, std::move(candidate), acc, 1,
                    max_formulas);
        }
      }
    }
  }

  // remove complex boolean functions now, they take longer to evaluate and make
  // for really complicated formulas as is.
  for (auto itr{interesting_bool_funcs.begin()};
       itr != interesting_bool_funcs.end();) {
    if ((*itr)->size() > max_bool_func_size) {
      itr = interesting_bool_funcs.erase(itr);
    } else {
      ++itr;
    }
  }
  cout << "num further reduced interesting bool funcs: "
       << interesting_bool_funcs.size() << "\n";

#pragma omp parallel for schedule(dynamic)
  for (auto &operand1 : interesting_bool_funcs) {
    for (auto &operand2 : interesting_bool_funcs) {
      if (operand1 == operand2) {
        continue;
      }
      for (size_t lb = 0; lb <= max_ub; lb += bounds_step) {
        for (size_t ub = lb + bounds_step; ub <= max_ub; ub += bounds_step) {
          shared_ptr<ASTNode> candidate =
              make_shared<Until>(operand1, operand2, lb, ub);
          float acc =
              calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
          {
            keep_best(formulas_best, formulas_worst, std::move(candidate), acc,
                      1, max_formulas);
          }

          candidate = make_shared<Release>(operand1, operand2, lb, ub);
          acc = calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
          {
            keep_best(formulas_best, formulas_worst, std::move(candidate), acc,
                      1, max_formulas);
          }
        }
      }
    }
  }

  for (int depth = 2; depth <= max_depth; ++depth) {
    cout << "GENERATING DEPTH " << depth << " FUNCTIONS\n";
    boost::container::flat_set<NodeWrapper> candidates_best;
    boost::container::flat_set<NodeWrapper, std::greater<NodeWrapper>>
        candidates_worst;
#pragma omp parallel for schedule(dynamic)
    for (auto &operand1 : formulas_best) {
      shared_ptr<ASTNode> candidate;
      float acc;
      for (size_t lb = 0; lb <= max_ub; lb += bounds_step) {
        for (size_t ub = lb + bounds_step; ub <= max_ub; ub += bounds_step) {
          if (operand1.ast->future_reach() + ub > max_pos_train_trace_len) {
            continue;
          }
          if (operand1.depth == depth - 1) {
            candidate = make_shared<Globally>(operand1.ast, lb, ub);
            acc = calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
            {
              keep_best(candidates_best, candidates_worst, std::move(candidate),
                        acc, depth, max_formulas);
            }

            candidate = make_shared<Finally>(operand1.ast, lb, ub);
            acc = calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
            {
              keep_best(candidates_best, candidates_worst, std::move(candidate),
                        acc, depth, max_formulas);
            }
          }

          for (auto &operand2 : formulas_best) {
            if (operand1.ast == operand2.ast) {
              continue;
            }
            if (operand1.depth < depth - 1 && operand2.depth < depth - 1) {
              continue;
            }
            if (operand2.ast->future_reach() + ub > max_pos_train_trace_len) {
              continue;
            }

            candidate = make_shared<Until>(operand1.ast, operand2.ast, lb, ub);
            acc = calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
            {
              keep_best(candidates_best, candidates_worst, std::move(candidate),
                        acc, depth, max_formulas);
            }

            candidate =
                make_shared<Release>(operand1.ast, operand2.ast, lb, ub);
            acc = calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
            {
              keep_best(candidates_best, candidates_worst, std::move(candidate),
                        acc, depth, max_formulas);
            }
          }

          if (operand1.depth == depth - 1) {
            // (2) GENERATE FORMULAS WITH AT LEAST ONE DEPTH -1 formula.
            for (auto &operand2 : interesting_bool_funcs) {
              candidate = make_shared<Until>(operand1.ast, operand2, lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates_best, candidates_worst,
                          std::move(candidate), acc, depth, max_formulas);
              }

              candidate = make_shared<Release>(operand1.ast, operand2, lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates_best, candidates_worst,
                          std::move(candidate), acc, depth, max_formulas);
              }

              candidate = make_shared<Until>(operand2, operand1.ast, lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates_best, candidates_worst,
                          std::move(candidate), acc, depth, max_formulas);
              }

              candidate = make_shared<Release>(operand2, operand1.ast, lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates_best, candidates_worst,
                          std::move(candidate), acc, depth, max_formulas);
              }

              // use some binary propositional operations now.
              candidate = make_shared<Globally>(
                  make_shared<And>(operand1.ast, operand2), lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates_best, candidates_worst,
                          std::move(candidate), acc, depth, max_formulas);
              }
              candidate = make_shared<Finally>(
                  make_shared<And>(operand1.ast, operand2), lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates_best, candidates_worst,
                          std::move(candidate), acc, depth, max_formulas);
              }
              candidate = make_shared<Globally>(
                  make_shared<Or>(operand1.ast, operand2), lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates_best, candidates_worst,
                          std::move(candidate), acc, depth, max_formulas);
              }
              candidate = make_shared<Finally>(
                  make_shared<Or>(operand1.ast, operand2), lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates_best, candidates_worst,
                          std::move(candidate), acc, depth, max_formulas);
              }

              // NEGATED
              // use some binary propositional operations now.
              if (operand2->get_type() !=
                  ASTNode::Type::Negation) { // don't double negate
                candidate = make_shared<Globally>(
                    make_shared<And>(operand1.ast,
                                     make_shared<Negation>(operand2)),
                    lb, ub);
                acc = calc_accuracy(*candidate, traces_pos_train,
                                    traces_neg_train);
#pragma omp critical
                {
                  keep_best(candidates_best, candidates_worst,
                            std::move(candidate), acc, depth, max_formulas);
                }
                candidate = make_shared<Finally>(
                    make_shared<And>(operand1.ast,
                                     make_shared<Negation>(operand2)),
                    lb, ub);
                acc = calc_accuracy(*candidate, traces_pos_train,
                                    traces_neg_train);
#pragma omp critical
                {
                  keep_best(candidates_best, candidates_worst,
                            std::move(candidate), acc, depth, max_formulas);
                }
                candidate = make_shared<Globally>(
                    make_shared<Or>(operand1.ast,
                                    make_shared<Negation>(operand2)),
                    lb, ub);
                acc = calc_accuracy(*candidate, traces_pos_train,
                                    traces_neg_train);
#pragma omp critical
                {
                  keep_best(candidates_best, candidates_worst,
                            std::move(candidate), acc, depth, max_formulas);
                }
                candidate = make_shared<Finally>(
                    make_shared<Or>(operand1.ast,
                                    make_shared<Negation>(operand2)),
                    lb, ub);
                acc = calc_accuracy(*candidate, traces_pos_train,
                                    traces_neg_train);
#pragma omp critical
                {
                  keep_best(candidates_best, candidates_worst,
                            std::move(candidate), acc, depth, max_formulas);
                }
              }
            }
          }

          // (integrate and/or?)
        }
      }
    }

    // BEGIN USING WORST
    /*
#pragma omp parallel for schedule(dynamic)
    for (auto &operand1 : formulas_worst) {
      shared_ptr<ASTNode> candidate;
      float acc;
      for (size_t lb = 0; lb <= max_ub; lb += bounds_step) {
        for (size_t ub = lb + bounds_step; ub <= max_ub; ub += bounds_step) {
          if (operand1.ast->future_reach() + ub > max_pos_train_trace_len) {
            continue;
          }
          if (operand1.depth == depth - 1) {
            candidate = make_shared<Globally>(operand1.ast, lb, ub);
            acc = calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
            {
              keep_best(candidates_best, candidates_worst, std::move(candidate),
                        acc, depth, max_formulas);
            }

            candidate = make_shared<Finally>(operand1.ast, lb, ub);
            acc = calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
            {
              keep_best(candidates_best, candidates_worst, std::move(candidate),
                        acc, depth, max_formulas);
            }
          }

          for (auto &operand2 : formulas_best) {
            if (operand1.ast == operand2.ast) {
              continue;
            }
            if (operand1.depth < depth - 1 && operand2.depth < depth - 1) {
              continue;
            }
            if (operand2.ast->future_reach() + ub > max_pos_train_trace_len) {
              continue;
            }

            candidate = make_shared<Until>(operand1.ast, operand2.ast, lb, ub);
            acc = calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
            {
              keep_best(candidates_best, candidates_worst, std::move(candidate),
                        acc, depth, max_formulas);
            }

            candidate =
                make_shared<Release>(operand1.ast, operand2.ast, lb, ub);
            acc = calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
            {
              keep_best(candidates_best, candidates_worst, std::move(candidate),
                        acc, depth, max_formulas);
            }

            candidate = make_shared<Until>(operand2.ast, operand1.ast, lb, ub);
            acc = calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
            {
              keep_best(candidates_best, candidates_worst, std::move(candidate),
                        acc, depth, max_formulas);
            }

            candidate =
                make_shared<Release>(operand2.ast, operand1.ast, lb, ub);
            acc = calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
            {
              keep_best(candidates_best, candidates_worst, std::move(candidate),
                        acc, depth, max_formulas);
            }
          }

          if (operand1.depth == depth - 1) {
            // (2) GENERATE FORMULAS WITH AT LEAST ONE DEPTH -1 formula.
            for (auto &operand2 : interesting_bool_funcs) {
              candidate = make_shared<Until>(operand1.ast, operand2, lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates_best, candidates_worst,
                          std::move(candidate), acc, depth, max_formulas);
              }

              candidate = make_shared<Release>(operand1.ast, operand2, lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates_best, candidates_worst,
                          std::move(candidate), acc, depth, max_formulas);
              }

              candidate = make_shared<Until>(operand2, operand1.ast, lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates_best, candidates_worst,
                          std::move(candidate), acc, depth, max_formulas);
              }

              candidate = make_shared<Release>(operand2, operand1.ast, lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates_best, candidates_worst,
                          std::move(candidate), acc, depth, max_formulas);
              }

              // use some binary propositional operations now.
              candidate = make_shared<Globally>(
                  make_shared<And>(operand1.ast, operand2), lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates_best, candidates_worst,
                          std::move(candidate), acc, depth, max_formulas);
              }
              candidate = make_shared<Finally>(
                  make_shared<And>(operand1.ast, operand2), lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates_best, candidates_worst,
                          std::move(candidate), acc, depth, max_formulas);
              }
              candidate = make_shared<Globally>(
                  make_shared<Or>(operand1.ast, operand2), lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates_best, candidates_worst,
                          std::move(candidate), acc, depth, max_formulas);
              }
              candidate = make_shared<Finally>(
                  make_shared<Or>(operand1.ast, operand2), lb, ub);
              acc =
                  calc_accuracy(*candidate, traces_pos_train, traces_neg_train);
#pragma omp critical
              {
                keep_best(candidates_best, candidates_worst,
                          std::move(candidate), acc, depth, max_formulas);
              }

              // NEGATED
              // use some binary propositional operations now.
              if (operand2->get_type() !=
                  ASTNode::Type::Negation) { // don't double negate
                candidate = make_shared<Globally>(
                    make_shared<And>(operand1.ast,
                                     make_shared<Negation>(operand2)),
                    lb, ub);
                acc = calc_accuracy(*candidate, traces_pos_train,
                                    traces_neg_train);
#pragma omp critical
                {
                  keep_best(candidates_best, candidates_worst,
                            std::move(candidate), acc, depth, max_formulas);
                }
                candidate = make_shared<Finally>(
                    make_shared<And>(operand1.ast,
                                     make_shared<Negation>(operand2)),
                    lb, ub);
                acc = calc_accuracy(*candidate, traces_pos_train,
                                    traces_neg_train);
#pragma omp critical
                {
                  keep_best(candidates_best, candidates_worst,
                            std::move(candidate), acc, depth, max_formulas);
                }
                candidate = make_shared<Globally>(
                    make_shared<Or>(operand1.ast,
                                    make_shared<Negation>(operand2)),
                    lb, ub);
                acc = calc_accuracy(*candidate, traces_pos_train,
                                    traces_neg_train);
#pragma omp critical
                {
                  keep_best(candidates_best, candidates_worst,
                            std::move(candidate), acc, depth, max_formulas);
                }
                candidate = make_shared<Finally>(
                    make_shared<Or>(operand1.ast,
                                    make_shared<Negation>(operand2)),
                    lb, ub);
                acc = calc_accuracy(*candidate, traces_pos_train,
                                    traces_neg_train);
#pragma omp critical
                {
                  keep_best(candidates_best, candidates_worst,
                            std::move(candidate), acc, depth, max_formulas);
                }
              }
            }
          }

          // (integrate and/or?)
        }
      }
    }
    */
    // END USING WORST

    formulas_best.merge(candidates_best);
    formulas_worst.merge(candidates_worst);
    // cut fat, now get rid of the worst 50% of formulas to avoid growing the
    // state space
    while (formulas_best.size() > max_formulas) {
      formulas_best.erase(formulas_best.begin());
    }
    while (formulas_worst.size() > max_formulas) {
      formulas_worst.erase(formulas_worst.begin());
    }
  }

  /* GenerateFormulas METHOD
  size_t begin_prev_depth = 0;
  size_t bounds_step = max_pos_train_trace_len / 4;
  size_t max_ub = max_pos_train_trace_len - 1;
  vector<shared_ptr<ASTNode>> formulas;

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
  size_t best_start_idx = formulas_best.size() - 1 - 10; // top 10
  size_t idx = 0;
  boost::container::flat_set<tuple<string, float, float>, TestAccuracyCompare>
      formulas_by_test_acc;
  boost::container::flat_set<tuple<string, float, float>,
                             WorstTestAccuracyCompare>
      formulas_by_worst_test_acc;

  cout << "\n\nWORST TRAIN ACCURACY:\n";
  for (const NodeWrapper &wrapper : formulas_worst) {
    float test_acc =
        calc_accuracy(*wrapper.ast, traces_pos_test, traces_neg_test);
    string formula_str = wrapper.ast->as_pretty_string();
    if (idx >= best_start_idx || wrapper.accuracy == 0) {
      cout << formula_str << "\n";
      cout << "  train accuracy: " << wrapper.accuracy << "\n";
      cout << "  test accuracy : " << test_acc << "\n";
    }
    formulas_by_worst_test_acc.emplace(formula_str, wrapper.accuracy, test_acc);
    ++idx;
  }
  idx = 0;
  cout << "\n\nWORST TEST ACCURACY:\n";
  for (const auto &result : formulas_by_worst_test_acc) {
    if (idx >= best_start_idx || std::get<2>(result) == 1) {
      cout << std::get<0>(result) << "\n";
      cout << "  train accuracy: " << std::get<1>(result) << "\n";
      cout << "  test accuracy : " << std::get<2>(result) << "\n";
    }
    ++idx;
  }

  idx = 0;
  cout << "\n\nBEST TRAIN ACCURACY:\n";
  for (const NodeWrapper &wrapper : formulas_best) {
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

  size_t num_perfect = 0;

  idx = 0;
  cout << "\n\nBEST TEST ACCURACY:\n";
  for (const auto &result : formulas_by_test_acc) {
    if (idx >= best_start_idx || std::get<2>(result) == 1) {
      if (std::get<2>(result) == 1) {
        ++num_perfect;
      }
      cout << std::get<0>(result) << "\n";
      cout << "  train accuracy: " << std::get<1>(result) << "\n";
      cout << "  test accuracy : " << std::get<2>(result) << "\n";
    }
    ++idx;
  }

  cout << "num_boolean_functions: " << num_boolean_functions << "\n";
  cout << "num reduced interesting bool funcs: "
       << interesting_bool_funcs.size() << "\n";
  cout << "num best formulas: " << formulas_best.size() << "\n";
  cout << "num worst formulas: " << formulas_worst.size() << "\n";
  cout << "num_perfect: " << num_perfect << "\n";
  cout << "total time taken: " << time_taken << "s\n";

  return 0;
}
