#include "ast.hh"

bool Finally::evaluate_subt(const vector<string> &trace, size_t begin,
                            size_t end) const {
  size_t subt_len = end - begin;
  if (subt_len <= lb) {
    return false;
  }
  size_t idx_lb = begin + lb;
  size_t idx_ub = begin + ub;
  size_t idx_end = min(idx_ub + 1, end);
  for (size_t i = idx_lb; i < idx_end; ++i) {
    if (operand->evaluate_subt(trace, i, end)) {
      return true;
    }
  }
  return false;
}

bool Globally::evaluate_subt(const vector<string> &trace, size_t begin,
                             size_t end) const {
  size_t subt_len = end - begin;
  if (subt_len <= lb) {
    return true;
  }
  size_t idx_lb = begin + lb;
  size_t idx_ub = begin + ub;
  size_t idx_end = min(idx_ub + 1, end);
  for (size_t i = idx_lb; i < idx_end; ++i) {
    if (!operand->evaluate_subt(trace, i, end)) {
      return false;
    }
  }
  return true;
}

bool Until::evaluate_subt(const vector<string> &trace, size_t begin,
                          size_t end) const {
  size_t subt_len = end - begin;
  if (subt_len <= lb) {
    return false;
  }
  size_t idx_lb = begin + lb;
  size_t idx_ub = begin + ub;
  size_t idx_end = min(idx_ub + 1, end);
  size_t i = -1;
  for (size_t k = idx_lb; k < idx_end; ++k) {
    if (right->evaluate_subt(trace, k, end)) {
      i = k;
      break;
    }
  }
  if (i == (size_t)-1) {
    return false;
  }
  for (size_t j = idx_lb; j < i; ++j) {
    if (!left->evaluate_subt(trace, j, end)) {
      return false;
    }
  }
  return true;
}

bool Release::evaluate_subt(const vector<string> &trace, size_t begin,
                            size_t end) const {
  size_t subt_len = end - begin;
  if (subt_len <= lb) {
    return true;
  }
  size_t idx_lb = begin + lb;
  size_t idx_ub = begin + ub;
  size_t idx_end = min(idx_ub + 1, end);
  size_t i;
  for (i = idx_lb; i < idx_end; ++i) {
    if (!right->evaluate_subt(trace, i, end)) {
      break;
    }
  }
  if (i == idx_end) {
    return true;
  }
  size_t j = -1;
  size_t k;
  for (k = idx_lb; k < idx_end; ++k) {
    if (left->evaluate_subt(trace, k, end)) {
      j = k + 1;
      break;
    }
  }
  if (k == idx_end) {
    j = k + 1;
  } else if (j == (size_t)-1) {
    return false;
  }
  idx_end = min(j, end);
  for (k = idx_lb; k < idx_end; ++k) {
    if (!right->evaluate_subt(trace, k, end)) {
      return false;
    }
  }
  return true;
}
