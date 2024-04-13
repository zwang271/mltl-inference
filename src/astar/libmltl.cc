#include "libmltl.hh"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

bool matches_in_set(const string &f, size_t pos, size_t len,
                    const vector<string> &targets) {
  for (const string &target : targets) {
    if (target.length() != len) {
      continue;
    } else if (!strncmp(f.c_str() + pos, target.c_str(), len)) {
      return true;
    }
  }
  return false;
}

bool is_valid_num(const string &f, size_t pos, size_t len) {
  size_t end = pos + len;
  for (; pos < end; ++pos) {
    if (!isdigit(f[pos])) {
      return false;
    }
  }
  return true;
}

/* Searches for matching closing parenthesis to the opening parenthesis expected
 * at pos. Returns the length of the statement capture inside of the parens.
 * Exits on error.
 *
 * Assumes f[pos] == '('
 */
size_t captured_stmt_len(const string &f, size_t pos, size_t len) {
  int pcount = 1;
  size_t begin = pos;
  size_t end = pos + len;
  for (pos = pos + 1; pos < end; ++pos) {
    pcount += (f[pos] == '(');
    pcount -= (f[pos] == ')');
  }

  if (pcount) {
    cout << "error: unbalanced parenthesis, expected ')' at "
         << f.substr(pos, len) << "\n";
    exit(1);
  }

  return pos - begin - 2;
}

/* Parses MLTL formula f, starts at character position pos and spans len
 * characters.
 */
MLTLNode *parse(const string &f, size_t pos, size_t len) {
  unsigned int id;
  size_t lb, ub, tmp;
  cout << "[debug]: f: " << f.substr(pos,len) << "\n";
  cout << "[debug]: pos: " << pos << "\n";
  cout << "[debug]: len: " << len << "\n";

  // attempt to parse as a non-compound statement
  switch (f[pos]) {
  case 't':
    if (matches_in_set(f, pos, len, {"t", "tt", "true"})) {
      return new MLTLPropConsNode(true);
    }
    break;
  case 'f':
    if (matches_in_set(f, pos, len, {"f", "ff", "false"})) {
      return new MLTLPropConsNode(false);
    }
    break;
  case 'p':
    if (is_valid_num(f, pos + 1, len - 1)) {
      id = stoul(f.substr(pos + 1, len - 1));
      return new MLTLPropVarNode(id);
    }
    break;
  case '(':
    tmp = captured_stmt_len(f, pos, len);
    cout << "[debug]: tmp: " << tmp << "\n";
    if (tmp + 2 == len) {
      // The whole string is encased in a set of parens, strip them.
      return parse(f, pos + 1, tmp);
    }
    break;
  default:
    // statement is a compound statement
    break;
  }

  // attempt to parse as a compound statement
  //
  // // implies or equivalence (->,<-,<->)
  // case '-':
  //   if (f[pos + 1] != '-') {
  //     cout << "error: unexpected token " << f.substr(pos, len) << "\n";
  //     exit(1);
  //   }
  //   break;
  // case '<':
  //   if (f[pos + 1] != '-') {
  //     cout << "error: unexpected token " << f.substr(pos, len) << "\n";
  //     exit(1);
  //   }
  //   if (f[pos + 2 == '>']) {
  //     // equiv
  //   }
  //   break;
  //
  // default:
  //   cout << "error: unexpected token " << f.substr(pos, len) << "\n";
  //   exit(1);
  // }

  cout << "error: unexpected token at " << f.substr(pos, len) << "\n";
  exit(1);
}

/* Fast recursive decent parser for MLTL.
 */
MLTLNode *parse(const string &f) {
  string tmp = f;
  // trim whitespace
  tmp.erase(remove_if(tmp.begin(), tmp.end(),
                      [](unsigned char c) { return isspace(c); }),
            tmp.end());
  return parse(tmp, 0, tmp.length());
}

/* Helper function to slice a given vector from range x to y.
 */
vector<string> slice(const vector<string> &a, int x, int y) {
  auto start = a.begin() + x;
  auto end = a.begin() + y;

  vector<string> result(y - x);
  copy(start, end, result.begin());
  return result;
}

/* class MLTLNode
 */
MLTLNode::MLTLNode(MLTLNodeType type) : type(type) {}

MLTLNode::~MLTLNode() {}

MLTLNodeType MLTLNode::getType() const { return type; }

/* class MLTLPropConsNode : public MLTLNode
 */
MLTLPropConsNode::MLTLPropConsNode(bool val)
    : MLTLNode(MLTLNodeType::PropCons), val(val) {}

MLTLPropConsNode::~MLTLPropConsNode() {}

string MLTLPropConsNode::as_string() const { return val ? "true" : "false"; }

size_t MLTLPropConsNode::future_reach() const { return 0; }

bool MLTLPropConsNode::evaluate(vector<string> &trace) const { return val; }

size_t MLTLPropConsNode::size() const { return 1; }
size_t MLTLPropConsNode::count(MLTLNodeType target_type) const {
  return (target_type == type);
}
size_t MLTLPropConsNode::count(MLTLUnaryPropOpType target_type) const {
  return 0;
}
size_t MLTLPropConsNode::count(MLTLBinaryPropOpType target_type) const {
  return 0;
}
size_t MLTLPropConsNode::count(MLTLUnaryTempOpType target_type) const {
  return 0;
}
size_t MLTLPropConsNode::count(MLTLBinaryTempOpType target_type) const {
  return 0;
}

/* class MLTLPropVarNode : public MLTLNode
 */
MLTLPropVarNode::MLTLPropVarNode(unsigned int var_id)
    : MLTLNode(MLTLNodeType::PropVar), var_id(var_id) {}

MLTLPropVarNode::~MLTLPropVarNode() {}

string MLTLPropVarNode::as_string() const { return 'p' + to_string(var_id); }

size_t MLTLPropVarNode::future_reach() const { return 1; }

bool MLTLPropVarNode::evaluate(vector<string> &trace) const {
  if (trace.size() == 0) {
    return false;
  }
  if (var_id >= trace[0].length()) {
    cout << "error: propositional variable " << as_string()
         << " is out of bounds of the trace\n";
    exit(1);
  }
  return (trace[0][var_id] == '1');
}

size_t MLTLPropVarNode::size() const { return 1; }
size_t MLTLPropVarNode::count(MLTLNodeType target_type) const {
  return (target_type == type);
}
size_t MLTLPropVarNode::count(MLTLUnaryPropOpType target_type) const {
  return 0;
}
size_t MLTLPropVarNode::count(MLTLBinaryPropOpType target_type) const {
  return 0;
}
size_t MLTLPropVarNode::count(MLTLUnaryTempOpType target_type) const {
  return 0;
}
size_t MLTLPropVarNode::count(MLTLBinaryTempOpType target_type) const {
  return 0;
}

/* class MLTLUnaryPropOpNode : public MLTLNode
 */
MLTLUnaryPropOpNode::MLTLUnaryPropOpNode(MLTLUnaryPropOpType op_type,
                                         MLTLNode *operand)
    : MLTLNode(MLTLNodeType::UnaryPropOp), op_type(op_type), operand(operand) {}

MLTLUnaryPropOpNode::~MLTLUnaryPropOpNode() { delete operand; }

string MLTLUnaryPropOpNode::as_string() const {
  return '~' + operand->as_string();
}

size_t MLTLUnaryPropOpNode::future_reach() const {
  return operand->future_reach();
}

bool MLTLUnaryPropOpNode::evaluate(vector<string> &trace) const {
  return !operand->evaluate(trace);
}

size_t MLTLUnaryPropOpNode::size() const { return 1 + operand->size(); }
size_t MLTLUnaryPropOpNode::count(MLTLNodeType target_type) const {
  return (target_type == type) + operand->count(target_type);
}
size_t MLTLUnaryPropOpNode::count(MLTLUnaryPropOpType target_type) const {
  return (target_type == op_type) + operand->count(target_type);
}
size_t MLTLUnaryPropOpNode::count(MLTLBinaryPropOpType target_type) const {
  return operand->count(target_type);
}
size_t MLTLUnaryPropOpNode::count(MLTLUnaryTempOpType target_type) const {
  return operand->count(target_type);
}
size_t MLTLUnaryPropOpNode::count(MLTLBinaryTempOpType target_type) const {
  return operand->count(target_type);
}

/* class MLTLBinaryPropOpNode : public MLTLNode
 */
MLTLBinaryPropOpNode::MLTLBinaryPropOpNode(MLTLBinaryPropOpType op_type,
                                           MLTLNode *left, MLTLNode *right)
    : MLTLNode(MLTLNodeType::BinaryPropOp), op_type(op_type), left(left),
      right(right) {}

MLTLBinaryPropOpNode::~MLTLBinaryPropOpNode() {
  delete left;
  delete right;
}

string MLTLBinaryPropOpNode::as_string() const {
  string s = '(' + left->as_string() + ')';
  switch (op_type) {
  case MLTLBinaryPropOpType::And:
    s += '&';
    break;
  case MLTLBinaryPropOpType::Xor:
    s += '^';
    break;
  case MLTLBinaryPropOpType::Or:
    s += '|';
    break;
  case MLTLBinaryPropOpType::Implies:
    s += "->";
    break;
  case MLTLBinaryPropOpType::Equiv:
    s += "<->";
    break;
  default:
    s += '?';
  }
  s += '(' + right->as_string() + ')';
  return s;
}

size_t MLTLBinaryPropOpNode::future_reach() const {
  return max(left->future_reach(), right->future_reach());
}

bool MLTLBinaryPropOpNode::evaluate(vector<string> &trace) const {
  switch (op_type) {
  case MLTLBinaryPropOpType::And:
    return (left->evaluate(trace) && right->evaluate(trace));
  case MLTLBinaryPropOpType::Xor:
    return (left->evaluate(trace) != right->evaluate(trace));
  case MLTLBinaryPropOpType::Or:
    return (left->evaluate(trace) || right->evaluate(trace));
  case MLTLBinaryPropOpType::Implies:
    return (left->evaluate(trace) && !right->evaluate(trace));
  case MLTLBinaryPropOpType::Equiv:
    return (left->evaluate(trace) == right->evaluate(trace));
  default:
    cout << "error: unexpected binary propositional operator " << as_string()
         << "\n";
    exit(1);
  }
}

size_t MLTLBinaryPropOpNode::size() const {
  return 1 + left->size() + right->size();
}
size_t MLTLBinaryPropOpNode::count(MLTLNodeType target_type) const {
  return (target_type == type) + left->count(target_type) +
         right->count(target_type);
}
size_t MLTLBinaryPropOpNode::count(MLTLUnaryPropOpType target_type) const {
  return left->count(target_type) + right->count(target_type);
}
size_t MLTLBinaryPropOpNode::count(MLTLBinaryPropOpType target_type) const {
  return (target_type == op_type) + left->count(target_type) +
         right->count(target_type);
}
size_t MLTLBinaryPropOpNode::count(MLTLUnaryTempOpType target_type) const {
  return left->count(target_type) + right->count(target_type);
}
size_t MLTLBinaryPropOpNode::count(MLTLBinaryTempOpType target_type) const {
  return left->count(target_type) + right->count(target_type);
}

/* class MLTLUnaryTempOpNode : public MLTLNode
 */
MLTLUnaryTempOpNode::MLTLUnaryTempOpNode(MLTLUnaryTempOpType op_type, size_t lb,
                                         size_t ub, MLTLNode *operand)
    : MLTLNode(MLTLNodeType::UnaryTempOp), op_type(op_type), lb(lb), ub(ub),
      operand(operand) {}

MLTLUnaryTempOpNode::~MLTLUnaryTempOpNode() { delete operand; }

string MLTLUnaryTempOpNode::as_string() const {
  string s = "";
  switch (op_type) {
  case MLTLUnaryTempOpType::Finally:
    s += 'F';
    break;
  case MLTLUnaryTempOpType::Globally:
    s += 'G';
    break;
  default:
    s += '?';
  }
  s += '[' + to_string(lb) + ',' + to_string(ub) + "](" + operand->as_string() +
       ')';
  return s;
}

size_t MLTLUnaryTempOpNode::future_reach() const {
  return ub + operand->future_reach();
}

bool MLTLUnaryTempOpNode::evaluate(vector<string> &trace) const {
  size_t end, i;
  vector<string> sub_trace;
  switch (op_type) {
  case MLTLUnaryTempOpType::Finally:
    // trace |- F[a, b] operand iff |T| > a and there exists i in [a, b] such
    // that trace[i:] |- operand
    if (trace.size() <= lb) {
      return false;
    }
    end = min(ub, trace.size() - 1);
    for (i = lb; i <= end; ++i) {
      // |trace| > i
      sub_trace = slice(trace, i, trace.size());
      if (operand->evaluate(sub_trace)) {
        return true;
      }
    } // no i in [a, b] such that trace[i:] |- operand
    return false;
  case MLTLUnaryTempOpType::Globally:
    // trace |- G[a, b] operand iff |T| <= a or for all i in [a, b], trace[i:]
    // |- operand
    if (trace.size() <= lb) {
      return true;
    }
    end = min(ub, trace.size() - 1);
    for (i = lb; i <= end; ++i) {
      // |T| > i
      sub_trace = slice(trace, i, trace.size());
      if (!operand->evaluate(sub_trace)) {
        return false;
      }
    } // for all i in [a, b], trace[i:] |- operand
    return true;

  default:
    cout << "error: unexpected unary temporal operator " << as_string() << "\n";
    exit(1);
  }
}

size_t MLTLUnaryTempOpNode::size() const { return 1 + operand->size(); }
size_t MLTLUnaryTempOpNode::count(MLTLNodeType target_type) const {
  return (target_type == type) + operand->count(target_type);
}
size_t MLTLUnaryTempOpNode::count(MLTLUnaryPropOpType target_type) const {
  return operand->count(target_type);
}
size_t MLTLUnaryTempOpNode::count(MLTLBinaryPropOpType target_type) const {
  return operand->count(target_type);
}
size_t MLTLUnaryTempOpNode::count(MLTLUnaryTempOpType target_type) const {
  return (target_type == op_type) + operand->count(target_type);
}
size_t MLTLUnaryTempOpNode::count(MLTLBinaryTempOpType target_type) const {
  return operand->count(target_type);
}

/* class MLTLBinaryTempOpNode : public MLTLNode
 */
MLTLBinaryTempOpNode::MLTLBinaryTempOpNode(MLTLBinaryTempOpType op_type,
                                           size_t lb, size_t ub, MLTLNode *left,
                                           MLTLNode *right)
    : MLTLNode(MLTLNodeType::BinaryTempOp), op_type(op_type), lb(lb), ub(ub),
      left(left), right(right) {}

MLTLBinaryTempOpNode::~MLTLBinaryTempOpNode() {
  delete left;
  delete right;
}

string MLTLBinaryTempOpNode::as_string() const {
  string s = '(' + left->as_string() + ')';
  switch (op_type) {
  case MLTLBinaryTempOpType::Until:
    s += 'U';
    break;
  case MLTLBinaryTempOpType::Release:
    s += 'R';
    break;
  default:
    s += '?';
  }
  s += '[' + to_string(lb) + ',' + to_string(ub) + "](" + right->as_string() +
       ')';
  return s;
}

size_t MLTLBinaryTempOpNode::future_reach() const {
  return ub + max(left->future_reach(), right->future_reach());
}

bool MLTLBinaryTempOpNode::evaluate(vector<string> &trace) const {
  size_t end, i, j, k;
  vector<string> sub_trace;
  switch (op_type) {
  case MLTLBinaryTempOpType::Until:
    // trace |- left U[a,b] right iff |trace| > a and there exists i in [a,b]
    // such that (trace[i:] |- right and for all j in [a, i-1], race[j:] |-
    // left)
    if (trace.size() <= lb) {
      return false;
    }
    // find first occurrence for which T[i:] |- F2
    end = min(ub, trace.size() - 1);
    i = SIZE_MAX;
    for (k = lb; k <= end; ++k) {
      sub_trace = slice(trace, k, trace.size());
      if (right->evaluate(sub_trace)) {
        i = k;
        break;
      }
    } // no i in [a, b] such that trace[i:] |- right
    if (i == SIZE_MAX) {
      return false;
    }
    // check that for all j in [a, i-1], trace[j:] |- left
    for (j = lb; j < i; ++j) {
      sub_trace = slice(trace, j, trace.size());
      if (!left->evaluate(sub_trace)) {
        return false;
      }
    } // for all j in [a, i-1], trace[a:j] |- left
    return true;

  case MLTLBinaryTempOpType::Release:
    // trace |- left R[a,b] right iff |trace| <= a or for all i in [a, b]
    // trace[i:] |- right or (there exists j in [a, b-1] such that trace[j:]
    // |- left and for all k in [a, j], trace[k:] |- right)
    if (trace.size() <= lb) {
      return true;
    }
    // check if all i in [a, b] trace[i:] |- right
    end = min(ub, trace.size() - 1);
    for (i = lb; i <= ub; ++i) {
      sub_trace = slice(trace, i, trace.size());
      if (!right->evaluate(sub_trace)) {
        break;
      }
      if (i == end) {
        return true;
      }
    } // not all i in [a, b] trace[i:] |- right
    // find first occurrence of j in [a, b-1] for which trace[j:] |- left
    j = SIZE_MAX;
    for (k = lb; k < ub; ++k) {
      sub_trace = slice(trace, k, trace.size());
      if (left->evaluate(sub_trace) || k == trace.size() - 1) {
        j = k;
        break;
      }
    } // no j in [a, b-1] such that T[j:] |- left
    if (j == SIZE_MAX) {
      return false;
    }
    // check that for all k in [a, j], T[k:] |- right
    for (k = lb; k <= j; ++k) {
      sub_trace = slice(trace, k, trace.size());
      if (!right->evaluate(sub_trace)) {
        return false;
      }
      if (k == sub_trace.size() - 1) {
        break;
      }
    } // for all k in [a, j], trace[k:] |- right
    return true;

  default:
    cout << "error: unexpected binary temporal operator " << as_string()
         << "\n";
    exit(1);
  }
}

size_t MLTLBinaryTempOpNode::size() const {
  return 1 + left->size() + right->size();
}
size_t MLTLBinaryTempOpNode::count(MLTLNodeType target_type) const {
  return (target_type == type) + left->count(target_type) +
         right->count(target_type);
}
size_t MLTLBinaryTempOpNode::count(MLTLUnaryPropOpType target_type) const {
  return left->count(target_type) + right->count(target_type);
}
size_t MLTLBinaryTempOpNode::count(MLTLBinaryPropOpType target_type) const {
  return left->count(target_type) + right->count(target_type);
}
size_t MLTLBinaryTempOpNode::count(MLTLUnaryTempOpType target_type) const {
  return left->count(target_type) + right->count(target_type);
}
size_t MLTLBinaryTempOpNode::count(MLTLBinaryTempOpType target_type) const {
  return (target_type == op_type) + left->count(target_type) +
         right->count(target_type);
}
