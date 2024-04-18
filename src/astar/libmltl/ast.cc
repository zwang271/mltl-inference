#include "ast.hh"

/* Helper function to slice a given vector from range x to y.
 */
vector<string> slice(const vector<string> &a, int x, int y) {
  auto start = a.begin() + x;
  auto end = a.begin() + y;

  vector<string> result(y - x);
  copy(start, end, result.begin());
  return result;
}

bool Finally::evaluate(const vector<string> &trace) const {
  vector<string> sub_trace;
  // trace |- F[a, b] operand iff |T| > a and there exists i in [a, b] such
  // that trace[i:] |- operand
  if (trace.size() <= lb) {
    return false;
  }
  size_t end = min(ub, trace.size() - 1);
  for (size_t i = lb; i <= end; ++i) {
    // |trace| > i
    sub_trace = slice(trace, i, trace.size());
    if (operand->evaluate(sub_trace)) {
      return true;
    }
  } // no i in [a, b] such that trace[i:] |- operand
  return false;
}

bool Globally::evaluate(const vector<string> &trace) const {
  vector<string> sub_trace;
  // trace |- G[a, b] operand iff |T| <= a or for all i in [a, b], trace[i:]
  // |- operand
  if (trace.size() <= lb) {
    return true;
  }
  size_t end = min(ub, trace.size() - 1);
  for (size_t i = lb; i <= end; ++i) {
    // |T| > i
    sub_trace = slice(trace, i, trace.size());
    if (!operand->evaluate(sub_trace)) {
      return false;
    }
  } // for all i in [a, b], trace[i:] |- operand
  return true;
}

bool Until::evaluate(const vector<string> &trace) const {
  vector<string> sub_trace;
  // trace |- left U[a,b] right iff |trace| > a and there exists i in [a,b]
  // such that (trace[i:] |- right and for all j in [a, i-1], race[j:] |-
  // left)
  if (trace.size() <= lb) {
    return false;
  }
  // find first occurrence for which trace[i:] |- right
  size_t end = min(ub, trace.size() - 1);
  size_t i = -1;
  for (size_t k = lb; k <= end; ++k) {
    sub_trace = slice(trace, k, trace.size());
    if (right->evaluate(sub_trace)) {
      i = k;
      break;
    }
  } // no i in [a, b] such that trace[i:] |- right
  if (i == (size_t)-1) {
    return false;
  }
  // check that for all j in [a, i-1], trace[j:] |- left
  for (size_t j = lb; j < i; ++j) {
    sub_trace = slice(trace, j, trace.size());
    if (!left->evaluate(sub_trace)) {
      return false;
    }
  } // for all j in [a, i-1], trace[a:j] |- left
  return true;
}

bool Release::evaluate(const vector<string> &trace) const {
  vector<string> sub_trace;
  // trace |- left R[a,b] right iff |trace| <= a or for all i in [a, b]
  // trace[i:] |- right or (there exists j in [a, b-1] such that trace[j:]
  // |- left and for all k in [a, j], trace[k:] |- right)
  if (trace.size() <= lb) {
    return true;
  }
  // check if all i in [a, b] trace[i:] |- right
  size_t end = min(ub, trace.size() - 1);
  for (size_t i = lb; i <= ub; ++i) {
    sub_trace = slice(trace, i, trace.size());
    if (!right->evaluate(sub_trace)) {
      break;
    }
    if (i == end) {
      return true;
    }
  } // not all i in [a, b] trace[i:] |- right
  // find first occurrence of j in [a, b-1] for which trace[j:] |- left
  size_t j = -1;
  for (size_t k = lb; k < ub; ++k) {
    sub_trace = slice(trace, k, trace.size());
    if (left->evaluate(sub_trace) || k == trace.size() - 1) {
      j = k;
      break;
    }
  } // no j in [a, b-1] such that T[j:] |- left
  if (j == (size_t)-1) {
    return false;
  }
  // check that for all k in [a, j], T[k:] |- right
  for (size_t k = lb; k <= j; ++k) {
    sub_trace = slice(trace, k, trace.size());
    if (!right->evaluate(sub_trace)) {
      return false;
    }
    if (k == sub_trace.size() - 1) {
      break;
    }
  } // for all k in [a, j], trace[k:] |- right
  return true;
}
