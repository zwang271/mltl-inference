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
  float priority;
  float accuracy;
  size_t size;
  NodeWrapper(unique_ptr<ASTNode> ast, float priority, float accuracy)
      : ast(std::move(ast)), priority(priority) {
    size = this->ast->size();
  };

  // determines position in set, break ties with ast lexicographical compare.
  bool operator<(const NodeWrapper &rhs) const {
    if (priority != rhs.priority) {
      return priority < rhs.priority;
    }
    return *ast < *rhs.ast;
  }
};

float h_dist(const NodeWrapper &node) { return abs(node.accuracy - 0.5); }
float h_size(const NodeWrapper &node) { return node.size; }

/*
bool best_first_search(vector<unique_ptr<ASTNode>> &bool_funcs, int *numNodes,
                       function<float(const NodeWrapper &)> h = nullptr) {
  *numNodes = 0;
  set<NodeWrapper> visited;
  set<NodeWrapper> pq;
  // visited.emplace(p);
  // pq.emplace(p);

  while (!pq.empty()) {
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
  // string base_path = "../dataset/fmsd17_formula1";
  // string base_path = "../dataset/fmsd17_formula2";
  // string base_path = "../dataset/fmsd17_formula3";
  // string base_path = "../dataset/basic_global";
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

  const size_t max_vars = 3;
  const size_t num_vars =
      min((size_t)max_vars, traces_pos_train[0][0].length());
  const size_t num_vars_in_trace = traces_pos_train[0][0].length();
  const size_t truth_table_rows = pow(2, num_vars);
  size_t num_boolean_functions = pow(2, pow(2, num_vars));
  vector<string> inputs(truth_table_rows);
  // set<unique_ptr<ASTNode>, ASTNodeUniquePtrCompare> example_set;
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

  set<unique_ptr<ASTNode>, ASTNodeUniquePtrCompare> bool_funcs_set;

  num_boolean_functions = bool_funcs.size();
  for (size_t i = 0; i < num_boolean_functions; ++i) {
    bool_funcs_set.emplace(bool_funcs[i]->deep_copy());
  }

  cout << "size: " << bool_funcs.size() << "\n";

  for (int i = 1; i < input_variables.size(); ++i) {
    for (size_t j = 0; j < num_boolean_functions; ++j) {
      unique_ptr<ASTNode> new_func = bool_funcs[j]->deep_copy();
      unsigned int id = 0;
          cout << j << "\n";
      for (int k = input_variables[0].size() - 1; k > 0; --k) {
        if (new_func == nullptr) {
          cout << j << "f\n";
        }
        replaceVar(*new_func, id, input_variables[i][k]);
        bool_funcs_set.emplace(std::move(new_func));
        ++id;
      }
    }
  }

  // bool_funcs.clear();
  // bool_funcs.reserve(bool_funcs_set.size());
  while (!bool_funcs_set.empty()) {
    auto ast = bool_funcs_set.extract(bool_funcs_set.begin());
    bool_funcs.push_back(std::move(ast.value()));
  }
  num_boolean_functions = bool_funcs.size();

  // for (auto &formula : bool_funcs) {
  //   cout << formula->as_pretty_string() << "\n";
  // }
  //
  size_t bounds_step = max_pos_train_trace_len / 4;
  size_t max_ub = max_pos_train_trace_len - 1;
  vector<unique_ptr<ASTNode>> formulas;

  // notice bounds exclude the ends, which are true/false. We don't care about
  // them, they are uninteresting and only bloat the search space.
  for (size_t i = 1; i < num_boolean_functions - 1; ++i) {
    for (size_t lb = 0; lb <= max_ub; lb += bounds_step) {
      for (size_t ub = lb + bounds_step; ub <= max_ub; ub += bounds_step) {
        formulas.emplace_back(
            make_unique<Finally>(bool_funcs[i]->deep_copy(), lb, ub));
        formulas.emplace_back(
            make_unique<Globally>(bool_funcs[i]->deep_copy(), lb, ub));
        for (size_t j = 1; j < num_boolean_functions - 1; ++j) {
          if (i == j) {
            continue;
          }
          formulas.emplace_back(make_unique<Until>(
              bool_funcs[i]->deep_copy(), bool_funcs[j]->deep_copy(), lb, ub));
          formulas.emplace_back(make_unique<Release>(
              bool_funcs[i]->deep_copy(), bool_funcs[j]->deep_copy(), lb, ub));
        }
      }
    }
  }

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

  // for (auto &formula : formulas) {
  //   cout << formula->as_pretty_string() << "\n";
  // }
  cout << "num formulas: " << formulas.size() << "\n";
  // #pragma omp parallel for
  // for (int i = 0; i < num_boolean_functions; ++i) {
  //     string formula = "G[0,3](" + boolean_functions[i] + ")";
  //     bool val =
  //         evaluate_mltl(formula, {"0101", "1101", "0101", "1101"}, false);
  // }

#pragma omp parallel for num_threads(12)
  for (uint64_t i = 0; i < formulas.size(); ++i) {
    // dynamic_cast<MLTLUnaryTempOpNode>(bool_funcs[i])->child;
    int traces_satisified = 0;
    for (size_t j = 0; j < traces_pos_train.size(); ++j) {
      traces_satisified += formulas[i]->evaluate(traces_pos_train[j]);
    }
    for (size_t j = 0; j < traces_neg_train.size(); ++j) {
      traces_satisified += !formulas[i]->evaluate(traces_neg_train[j]);
    }
    float accuracy = traces_satisified /
                     (float)(traces_pos_train.size() + traces_neg_train.size());
    if (accuracy <= 0.34 || accuracy >= 0.66) {
      // if (accuracy >= 0.68) {
#pragma omp critical
      {
        cout << formulas[i]->as_pretty_string() << "\n";
        cout << "accuracy: " << accuracy << "\n";
      }
    }
  }

  gettimeofday(&end, NULL); // stop timer
  time_taken = end.tv_sec + end.tv_usec / 1e6 - start.tv_sec -
               start.tv_usec / 1e6; // in seconds
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
