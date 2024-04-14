#include "libmltl.hh"

#include <algorithm>
#include <cstring>
#include <iostream>

#define DEBUG 0

/* Precedence:
 *   0 : true false p#
 *   1 : F G ~
 *   2 : U R
 *   3 : &
 *   4 : ^
 *   5 : |
 *   6 : ->
 *   7 : <->
 */
constexpr int PropConsPrec = 0;
constexpr int PropVarPrec = 0;
constexpr int FinallyPrec = 1;
constexpr int GloballyPrec = 1;
constexpr int NegPrec = 1;
constexpr int UntilPrec = 2;
constexpr int ReleasePrec = 2;
constexpr int AndPrec = 3;
constexpr int XorPrec = 4;
constexpr int OrPrec = 5;
constexpr int ImpliesPrec = 6;
constexpr int EquivPrec = 7;

/* Emit diagnostic error and exit.
 */
[[noreturn]] void error(const string &msg, const string &f = "",
                        size_t pos = string::npos,
                        size_t ul_begin = string::npos,
                        size_t ul_end = string::npos) {
  cout << "error: " << msg << "\n";
  if (!f.empty()) {
    cout << f << "\n";
  }
  if (pos != string::npos) {
    size_t end;
    if (ul_begin != string::npos && ul_end != string::npos) {
      end = max(pos, ul_end);
    } else {
      end = pos;
    }
    for (size_t i = 0; i < end; ++i) {
      if (i == pos) {
        cout << '^';
      } else if (ul_begin <= i && i < ul_end) {
        cout << '~';
      } else {
        cout << ' ';
      }
    }
    cout << "\n";
  }

  exit(-1);
}

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
  if (len == 0) {
    return false;
  }
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
    if (pcount == 0 && pos != end - 1) {
      return pos - begin;
    }
  }

  if (pcount > 0) {
    error("unbalanced parenthesis, expected ')'", f, pos + len, pos,
          pos + len); // no return
  } else if (pcount < 0) {
    error("unbalanced parenthesis, expected '('", f, pos, pos,
          pos + len); // no return
  }

  return pos - begin - 2;
}

tuple<size_t, size_t> find_bounds(const string &f, size_t pos, size_t len,
                                  size_t *end_subscript = nullptr) {
  size_t lbrace = f.find('[', pos);
  size_t comma = f.find(',', pos);
  size_t rbrace = f.find(']', pos);
  size_t end = pos + len;
  if (lbrace >= end || comma >= end || rbrace >= end) {
    error("missing temporal operator bounds subscript", f, pos, pos,
          pos + len); // no return
  }
  size_t lb = stoull(f.substr(lbrace + 1, comma - lbrace - 1));
  size_t ub = stoull(f.substr(comma + 1, rbrace - comma - 1));
  if (lb > ub) {
    error("illegal temporal operator bounds subscript", f, pos, pos,
          rbrace + 1); // no return
  }
  if (end_subscript) {
    *end_subscript = rbrace + 1;
  }

  return make_tuple(lb, ub);
}

/* Returns the index of the binary operator with the lowest precedence.
 *
 * Precedence is determined as follows: (lower value = higher precedence)
 *   0 : true false p#
 *   1 : F G ~
 *   2 : U R
 *   3 : &
 *   4 : ^
 *   5 : |
 *   6 : ->
 *   7 : <->
 */
size_t find_lowest_prec_binary_op(const string &f, size_t pos, size_t len) {
  int pcount = 0;
  size_t begin = pos;
  size_t end = pos + len;
  size_t lowest_prec_pos = -1;
  int lowest_prec = -1;
  for (; pos < end; ++pos) {
    pcount += (f[pos] == '(');
    pcount -= (f[pos] == ')');
    if (pcount == 0) {
      // we are not inside parens, so this may be a low precedence operator
      switch (f[pos]) {
      case 'U':
        if (UntilPrec > lowest_prec) {
          lowest_prec_pos = pos;
          lowest_prec = UntilPrec;
        }
        break;
      case 'R':
        if (ReleasePrec > lowest_prec) {
          lowest_prec_pos = pos;
          lowest_prec = ReleasePrec;
        }
        break;
      case '&':
        if (AndPrec > lowest_prec) {
          lowest_prec_pos = pos;
          lowest_prec = AndPrec;
        }
        break;
      case '^':
        if (XorPrec > lowest_prec) {
          lowest_prec_pos = pos;
          lowest_prec = XorPrec;
        }
        break;
      case '|':
        if (OrPrec > lowest_prec) {
          lowest_prec_pos = pos;
          lowest_prec = OrPrec;
        }
        break;
      case '-':
        if (begin < pos && pos + 1 < end && f[pos - 1] != '<' &&
            f[pos + 1] == '>' && ImpliesPrec > lowest_prec) {
          lowest_prec_pos = pos;
          lowest_prec = ImpliesPrec;
        }
        break;
      case '<':
        if (!(pos + 2 < end && f[pos + 1] == '-' && f[pos + 2] == '>')) {
          break;
        }
      case '=':
        if (EquivPrec > lowest_prec) {
          lowest_prec_pos = pos;
          lowest_prec = EquivPrec;
        }
        break;
      default:
        break; // not an operator, continue the search
      }
    }
  }
  if (lowest_prec_pos == (size_t)-1) {
    // no binary operator found :(
    error("unexpected token", f, begin, begin,
          end); // no return
  }
  return lowest_prec_pos;
}

// forward declare
MLTLNode *parse_single_stmt(const string &f, size_t pos, size_t len);
MLTLNode *parse_compound_stmt(const string &f, size_t pos, size_t len);

/* Parses MLTL formula f, starts at character position pos and spans len
 * characters.
 */
MLTLNode *parse(const string &f, size_t pos, size_t len) {
  MLTLNode *ast;

  // try to parse as a single statement first, if that fails (returns nullptr),
  // then try to parse as a compound statement, if the fails then abort.
  ast = parse_single_stmt(f, pos, len);
  if (ast) {
    return ast;
  }
  ast = parse_compound_stmt(f, pos, len);
  if (ast) {
    return ast;
  }

  error("unexpected token", f, pos, pos, pos + len); // no return
  return nullptr;
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

/* Parses a single statement, returns nullptr is statement is a compound
 * statement.
 */
MLTLNode *parse_single_stmt(const string &f, size_t pos, size_t len) {
#if DEBUG
  cout << "[debug]: parse_single_stmt\n";
  cout << "[debug]:   f: " << f.substr(pos, len) << "\n";
  cout << "[debug]:   pos: " << pos << "\n";
  cout << "[debug]:   len: " << len << "\n";
#endif

  size_t end = pos + len;
  size_t lb, ub, captured_length, end_subscript;
  unsigned int id;
  MLTLNode *operand;
  switch (f[pos]) {
  case 't':
    if (len == 1 || (len == 2 && f[pos + 1] == 't') ||
        (len == 4 && f[pos + 1] == 'r' && f[pos + 2] == 'u' &&
         f[pos + 3] == 'e')) { // t, tt, true
      return new MLTLPropConsNode(true);
    }
    break;
  case 'f':
    if (len == 1 || (len == 2 && f[pos + 1] == 'f') ||
        (len == 5 && f[pos + 1] == 'a' && f[pos + 2] == 'l' &&
         f[pos + 3] == 's' && f[pos + 4] == 'e')) { // f, ff, false
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
    captured_length = captured_stmt_len(f, pos, len);
#if DEBUG
    cout << "[debug]:   captured_length: " << captured_length << "\n";
#endif
    if (captured_length + 2 == len) {
      // the whole string is encased in a set of parens, strip parse inside.
      return parse(f, pos + 1, captured_length);
    }
    break;
  case '~':
  case '!':
    operand = parse_single_stmt(f, pos + 1, len - 1);
    if (operand) {
      return new MLTLUnaryPropOpNode(MLTLUnaryPropOpType::Neg, operand);
    }
    break;
  case 'F':
    tie(lb, ub) = find_bounds(f, pos + 1, len - 1, &end_subscript);
    operand = parse_single_stmt(f, end_subscript, end - end_subscript);
    if (operand) {
      return new MLTLUnaryTempOpNode(MLTLUnaryTempOpType::Finally, lb, ub,
                                     operand);
    }
    break;
  case 'G':
    tie(lb, ub) = find_bounds(f, pos + 1, len - 1, &end_subscript);
    operand = parse_single_stmt(f, end_subscript, end - end_subscript);
    if (operand) {
      return new MLTLUnaryTempOpNode(MLTLUnaryTempOpType::Globally, lb, ub,
                                     operand);
    }
    break;
  default:
    // statement is a compound statement
    break;
  }
  return nullptr;
}

MLTLNode *parse_compound_stmt(const string &f, size_t pos, size_t len) {
#if DEBUG
  cout << "[debug]: parse_compound_stmt\n";
  cout << "[debug]:   f: " << f.substr(pos, len) << "\n";
  cout << "[debug]:   pos: " << pos << "\n";
  cout << "[debug]:   len: " << len << "\n";
#endif

  size_t end = pos + len;
  size_t lb, ub, end_subscript;
  MLTLNode *left, *right;
  size_t op_pos = find_lowest_prec_binary_op(f, pos, len);
  switch (f[op_pos]) {
  case 'U':
    tie(lb, ub) = find_bounds(f, op_pos + 1, len - 1, &end_subscript);
    left = parse(f, pos, op_pos - pos);
    right = parse(f, end_subscript, end - end_subscript);
    return new MLTLBinaryTempOpNode(MLTLBinaryTempOpType::Until, lb, ub, left,
                                    right);
  case 'R':
    tie(lb, ub) = find_bounds(f, op_pos + 1, len - 1, &end_subscript);
    left = parse(f, pos, op_pos - pos);
    right = parse(f, end_subscript, end - end_subscript);
    return new MLTLBinaryTempOpNode(MLTLBinaryTempOpType::Release, lb, ub, left,
                                    right);
  case '&':
    left = parse(f, pos, op_pos - pos);
    right = parse(f, op_pos + 1, end - op_pos - 1);
    return new MLTLBinaryPropOpNode(MLTLBinaryPropOpType::And, left, right);
  case '^':
    left = parse(f, pos, op_pos - pos);
    right = parse(f, op_pos + 1, end - op_pos - 1);
    return new MLTLBinaryPropOpNode(MLTLBinaryPropOpType::Xor, left, right);
  case '|':
    left = parse(f, pos, op_pos - pos);
    right = parse(f, op_pos + 1, end - op_pos - 1);
    return new MLTLBinaryPropOpNode(MLTLBinaryPropOpType::Or, left, right);
  case '-':
    if (pos < op_pos && op_pos + 1 < end && f[op_pos - 1] != '<' &&
        f[op_pos + 1] == '>') {
      left = parse(f, pos, op_pos - pos);
      right = parse(f, op_pos + 2, end - op_pos - 2);
      return new MLTLBinaryPropOpNode(MLTLBinaryPropOpType::Implies, left,
                                      right);
    }
    break;
  case '<':
    if (!(op_pos + 2 < end && f[op_pos + 1] == '-' && f[op_pos + 2] == '>')) {
      break;
    }
  case '=':
    left = parse(f, pos, op_pos - pos);
    right = parse(f, op_pos + 3, end - op_pos - 3);
    return new MLTLBinaryPropOpNode(MLTLBinaryPropOpType::Equiv, left, right);
  default:
    break; // not an operator, continue the search
  }
  return nullptr;
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

size_t MLTLPropConsNode::future_reach() const {
  // Mission-time LTL (MLTL) Formula Validation Via Regular Expressions
  // https://temporallogic.org/research/WEST/WEST_extended.pdf
  // Definition 6
  return 0;
}

bool MLTLPropConsNode::evaluate(const vector<string> &trace) const {
  return val;
}

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

size_t MLTLPropVarNode::future_reach() const {
  // Mission-time LTL (MLTL) Formula Validation Via Regular Expressions
  // https://temporallogic.org/research/WEST/WEST_extended.pdf
  // Definition 6
  return 1;
}

bool MLTLPropVarNode::evaluate(const vector<string> &trace) const {
  if (trace.size() == 0) {
    return false;
  }
  if (var_id >= trace[0].length()) {
    error("propositional variable " + as_string() +
          " is out of bounds on the trace"); // no return
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
  // Mission-time LTL (MLTL) Formula Validation Via Regular Expressions
  // https://temporallogic.org/research/WEST/WEST_extended.pdf
  // Definition 6
  return operand->future_reach();
}

bool MLTLUnaryPropOpNode::evaluate(const vector<string> &trace) const {
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
  // Mission-time LTL (MLTL) Formula Validation Via Regular Expressions
  // https://temporallogic.org/research/WEST/WEST_extended.pdf
  // Definition 6
  return max(left->future_reach(), right->future_reach());
}

bool MLTLBinaryPropOpNode::evaluate(const vector<string> &trace) const {
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
    error("unexpected binary propositional operator " +
          as_string()); // no return
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
  // Mission-time LTL (MLTL) Formula Validation Via Regular Expressions
  // https://temporallogic.org/research/WEST/WEST_extended.pdf
  // Definition 6
  return ub + operand->future_reach();
}

bool MLTLUnaryTempOpNode::evaluate(const vector<string> &trace) const {
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
    error("unexpected unary temporal operator " + as_string()); // no return
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
  // Mission-time LTL (MLTL) Formula Validation Via Regular Expressions
  // https://temporallogic.org/research/WEST/WEST_extended.pdf
  // Definition 6
  return ub + max(left->future_reach() - 1, right->future_reach());
}

bool MLTLBinaryTempOpNode::evaluate(const vector<string> &trace) const {
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
    // find first occurrence for which trace[i:] |- right
    end = min(ub, trace.size() - 1);
    i = -1;
    for (k = lb; k <= end; ++k) {
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
    j = -1;
    for (k = lb; k < ub; ++k) {
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
    error("unexpected binary temporal operator " + as_string()); // no return
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
