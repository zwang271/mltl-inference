#include "mltl_ast.h"

#include <climits>
#include <iostream>

/* Function to slice a given vector from range x to y
 */
vector<string> slice(vector<string> &a, int x, int y) {
  auto start = a.begin() + x;
  auto end = a.begin() + y;

  vector<string> result(y - x);
  copy(start, end, result.begin());
  return result;
}

// class MLTLNode
MLTLNode::MLTLNode(MLTLNodeType type) : type(type) {}

MLTLNodeType MLTLNode::getType() const { return type; }

// class MLTLPropConsNode : public MLTLNode
MLTLPropConsNode::MLTLPropConsNode(bool val)
    : MLTLNode(MLTLNodeType::PropCons), val(val) {}

string MLTLPropConsNode::as_string() const { return val ? "true" : "false"; }

unsigned int MLTLPropConsNode::future_reach() const { return 0; }

bool MLTLPropConsNode::evaluate(vector<string> &trace) const { return val; }

// class MLTLPropVarNode : public MLTLNode

MLTLPropVarNode::MLTLPropVarNode(unsigned int var_id)
    : MLTLNode(MLTLNodeType::PropVar), var_id(var_id) {}

string MLTLPropVarNode::as_string() const { return 'p' + to_string(var_id); }

unsigned int MLTLPropVarNode::future_reach() const { return 1; }

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

// class MLTLUnaryPropOpNode : public MLTLNode
MLTLUnaryPropOpNode::MLTLUnaryPropOpNode(MLTLUnaryPropOpType op_type,
                                         unique_ptr<MLTLNode> child)
    : MLTLNode(MLTLNodeType::UnaryPropOp), op_type(op_type),
      child(std::move(child)) {}

string MLTLUnaryPropOpNode::as_string() const {
  return '~' + child->as_string();
}

unsigned int MLTLUnaryPropOpNode::future_reach() const {
  return child->future_reach();
}

bool MLTLUnaryPropOpNode::evaluate(vector<string> &trace) const {
  return !child->evaluate(trace);
}

// class MLTLBinaryPropOpNode : public MLTLNode
MLTLBinaryPropOpNode::MLTLBinaryPropOpNode(MLTLBinaryPropOpType op_type,
                                           unique_ptr<MLTLNode> left,
                                           unique_ptr<MLTLNode> right)
    : MLTLNode(MLTLNodeType::BinaryPropOp), op_type(op_type),
      left(std::move(left)), right(std::move(right)) {}

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

unsigned int MLTLBinaryPropOpNode::future_reach() const {
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

// class MLTLUnaryTempOpNode : public MLTLNode
MLTLUnaryTempOpNode::MLTLUnaryTempOpNode(MLTLUnaryTempOpType op_type,
                                         unsigned int lb, unsigned int ub,
                                         unique_ptr<MLTLNode> child)
    : MLTLNode(MLTLNodeType::UnaryTempOp), op_type(op_type), lb(lb), ub(ub),
      child(std::move(child)) {}

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
  s += '[' + to_string(lb) + ',' + to_string(ub) + "](" + child->as_string() +
       ')';
  return s;
}

unsigned int MLTLUnaryTempOpNode::future_reach() const {
  return ub + child->future_reach();
}

bool MLTLUnaryTempOpNode::evaluate(vector<string> &trace) const {
  unsigned int end, i;
  vector<string> sub_trace;
  switch (op_type) {
  case MLTLUnaryTempOpType::Finally:
    // trace |- F[a, b] child iff |T| > a and there exists i in [a, b] such that
    // trace[i:] |- child
    if (trace.size() <= lb) {
      return false;
    }
    end = min(ub, (unsigned int)trace.size() - 1);
    for (i = lb; i <= end; ++i) {
      // |trace| > i
      sub_trace = slice(trace, i, trace.size());
      if (child->evaluate(sub_trace)) {
        return true;
      }
    } // no i in [a, b] such that trace[i:] |- child
    return false;
  case MLTLUnaryTempOpType::Globally:
    // trace |- G[a, b] child iff |T| <= a or for all i in [a, b], trace[i:] |-
    // child
    if (trace.size() <= lb) {
      return true;
    }
    end = min(ub, (unsigned int)trace.size() - 1);
    for (i = lb; i <= end; ++i) {
      // |T| > i
      sub_trace = slice(trace, i, trace.size());
      if (!child->evaluate(sub_trace)) {
        return false;
      }
    } // for all i in [a, b], trace[i:] |- child
    return true;

  default:
    cout << "error: unexpected unary temporal operator " << as_string() << "\n";
    exit(1);
  }
}

// class MLTLBinaryTempOpNode : public MLTLNode
MLTLBinaryTempOpNode::MLTLBinaryTempOpNode(MLTLBinaryTempOpType op_type,
                                           unsigned int lb, unsigned int ub,
                                           unique_ptr<MLTLNode> left,
                                           unique_ptr<MLTLNode> right)
    : MLTLNode(MLTLNodeType::BinaryTempOp), op_type(op_type), lb(lb), ub(ub),
      left(std::move(left)), right(std::move(right)) {}

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

unsigned int MLTLBinaryTempOpNode::future_reach() const {
  return ub + max(left->future_reach(), right->future_reach());
}

bool MLTLBinaryTempOpNode::evaluate(vector<string> &trace) const {
  unsigned int end, i, j, k;
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
    end = min(ub, (unsigned int)trace.size() - 1);
    i = UINT_MAX;
    for (k = lb; k <= end; ++k) {
      sub_trace = slice(trace, k, trace.size());
      if (right->evaluate(sub_trace)) {
        i = k;
        break;
      }
    } // no i in [a, b] such that trace[i:] |- right
    if (i == UINT_MAX) {
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
    // trace[i:] |- right or (there exists j in [a, b-1] such that trace[j:] |-
    // left and for all k in [a, j], trace[k:] |- right)
    if (trace.size() <= lb) {
      return true;
    }
    // check if all i in [a, b] trace[i:] |- right
    end = min(ub, (unsigned int)trace.size() - 1);
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
    j = UINT_MAX;
    for (k = lb; k < ub; ++k) {
      sub_trace = slice(trace, k, trace.size());
      if (left->evaluate(sub_trace) || k == trace.size() - 1) {
        j = k;
        break;
      }
    } // no j in [a, b-1] such that T[j:] |- left
    if (j == UINT_MAX) {
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
