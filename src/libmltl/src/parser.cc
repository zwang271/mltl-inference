#include "parser.hh"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stack>

#if DEBUG
#include <iostream>
#endif

using namespace std;
namespace fs = filesystem;
namespace libmltl {

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
// constexpr int ConstantPrec = 0;
// constexpr int VariablePrec = 0;
// constexpr int FinallyPrec = 1;
// constexpr int GloballyPrec = 1;
// constexpr int NegPrec = 1;
constexpr int UntilPrec = 2;
constexpr int ReleasePrec = 2;
constexpr int AndPrec = 3;
constexpr int XorPrec = 4;
constexpr int OrPrec = 5;
constexpr int ImpliesPrec = 6;
constexpr int EquivPrec = 7;

/* Emit diagnostic error and exit.
 */
void error(const string &msg, const string &f = "", size_t pos = string::npos,
           size_t ul_begin = string::npos, size_t ul_end = string::npos) {
  string diag = "\nerror: " + msg + "\n";
  if (!f.empty()) {
    diag += "  " + f + "\n";
  }
  if (pos != string::npos) {
    diag += "  ";
    size_t end;
    if (ul_begin != string::npos && ul_end != string::npos) {
      end = max(pos, ul_end);
    } else {
      end = pos;
    }
    for (size_t i = 0; i < end; ++i) {
      if (i == pos) {
        diag += '^';
      } else if (ul_begin <= i && i < ul_end) {
        diag += '~';
      } else {
        diag += ' ';
      }
    }
    // cout << "\n";
  }

  throw syntax_error(diag);
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
size_t find_lowest_prec_binary_op(const string &f, size_t pos, size_t len,
                                  const vector<size_t> &paren_map) {
  size_t begin = pos;
  size_t end = pos + len;
  size_t lowest_prec_pos = -1;
  int lowest_prec = -1;
  for (; pos < end; ++pos) {
    if (f[pos] == '(') {
      // skip over the parens, since they have highest precedence
      pos = paren_map[pos];
      continue;
    }
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
  if (lowest_prec_pos == (size_t)-1) {
    // no binary operator found :(
    error("unexpected token", f, begin, begin,
          end); // no return
  }
  return lowest_prec_pos;
}

// forward declare
unique_ptr<ASTNode> parse_single_stmt(const string &f, size_t pos, size_t len,
                                      const vector<size_t> &paren_map);
unique_ptr<ASTNode> parse_compound_stmt(const string &f, size_t pos, size_t len,
                                        const vector<size_t> &paren_map);

/* Parses MLTL formula f, starts at character position pos and spans len
 * characters.
 */
unique_ptr<ASTNode> parse(const string &f, size_t pos, size_t len,
                          const vector<size_t> &paren_map) {
  unique_ptr<ASTNode> ast;

  // try to parse as a single statement first, if that fails (returns
  // nullptr), then try to parse as a compound statement, if the fails then
  // abort.
  ast = parse_single_stmt(f, pos, len, paren_map);
  if (ast != nullptr) {
    return ast;
  }
  ast = parse_compound_stmt(f, pos, len, paren_map);
  if (ast != nullptr) {
    return ast;
  }

  error("unexpected token", f, pos, pos, pos + len); // no return
  return nullptr;
}

/* Fast recursive decent parser for MLTL.
 */
unique_ptr<ASTNode> parse(const string &formula) {
  string f = formula;
  // trim whitespace
  f.erase(
      remove_if(f.begin(), f.end(), [](unsigned char c) { return isspace(c); }),
      f.end());

  // first we generate a lookup table so finding matching parens is O(1)
  vector<size_t> paren_map(f.length(), 0);
  std::stack<size_t, std::vector<size_t>> paren_stack;

  for (size_t i = 0; i < f.length(); ++i) {
    if (f[i] == '(') {
      paren_stack.push(i);
    }
    if (f[i] == ')') {
      if (paren_stack.empty()) {
        error("unbalanced parenthesis, expected '('", f, i, 0,
              i + 1); // no return
      }
      size_t opening_paren_index = paren_stack.top();
      paren_map[opening_paren_index] = i;
      // we only ever use this map to find a matching closing paren
      // paren_map[i] = opening_paren_index;
      paren_stack.pop();
    }
  }

  if (!paren_stack.empty()) {
    size_t pos = paren_stack.top();
    error("unbalanced parenthesis, expected ')'", f, pos, pos,
          f.length()); // no return
  }

  return parse(f, 0, f.length(), paren_map);
}

/* Parses a single statement, returns nullptr is statement is a compound
 * statement.
 */
unique_ptr<ASTNode> parse_single_stmt(const string &f, size_t pos, size_t len,
                                      const vector<size_t> &paren_map) {
#if DEBUG
  cout << "[debug]: parse_single_stmt\n";
  cout << "[debug]:   f: " << f.substr(pos, len) << "\n";
  cout << "[debug]:   pos: " << pos << "\n";
  cout << "[debug]:   len: " << len << "\n";
#endif

  size_t end = pos + len;
  size_t lb, ub, captured_length, end_subscript;
  unsigned int id;
  unique_ptr<ASTNode> operand;
  switch (f[pos]) {
  case 't':
    if (len == 1 || (len == 2 && f[pos + 1] == 't') ||
        (len == 4 && f[pos + 1] == 'r' && f[pos + 2] == 'u' &&
         f[pos + 3] == 'e')) { // t, tt, true
      return make_unique<Constant>(true);
    }
    break;
  case 'f':
    if (len == 1 || (len == 2 && f[pos + 1] == 'f') ||
        (len == 5 && f[pos + 1] == 'a' && f[pos + 2] == 'l' &&
         f[pos + 3] == 's' && f[pos + 4] == 'e')) { // f, ff, false
      return make_unique<Constant>(false);
    }
    break;
  case 'p':
    if (is_valid_num(f, pos + 1, len - 1)) {
      id = stoul(f.substr(pos + 1, len - 1));
      return make_unique<Variable>(id);
    }
    break;
  case '(':
    captured_length = paren_map[pos] - pos - 1;
#if DEBUG
    cout << "[debug]:   captured_length: " << captured_length << "\n";
#endif
    if (captured_length + 2 == len) {
      // the whole string is encased in a set of parens, strip parse inside.
      return parse(f, pos + 1, captured_length, paren_map);
    }
    break;
  case '~':
  case '!':
    operand = parse_single_stmt(f, pos + 1, len - 1, paren_map);
    if (operand) {
      return make_unique<Negation>(std::move(operand));
    }
    break;
  case 'F':
    tie(lb, ub) = find_bounds(f, pos + 1, len - 1, &end_subscript);
    operand =
        parse_single_stmt(f, end_subscript, end - end_subscript, paren_map);
    if (operand) {
      return make_unique<Finally>(std::move(operand), lb, ub);
    }
    break;
  case 'G':
    tie(lb, ub) = find_bounds(f, pos + 1, len - 1, &end_subscript);
    operand =
        parse_single_stmt(f, end_subscript, end - end_subscript, paren_map);
    if (operand) {
      return make_unique<Globally>(std::move(operand), lb, ub);
    }
    break;
  default:
    // statement is a compound statement
    break;
  }
  return nullptr;
}

unique_ptr<ASTNode> parse_compound_stmt(const string &f, size_t pos, size_t len,
                                        const vector<size_t> &paren_map) {
#if DEBUG
  cout << "[debug]: parse_compound_stmt\n";
  cout << "[debug]:   f: " << f.substr(pos, len) << "\n";
  cout << "[debug]:   pos: " << pos << "\n";
  cout << "[debug]:   len: " << len << "\n";
#endif

  size_t end = pos + len;
  size_t lb, ub, end_subscript;
  unique_ptr<ASTNode> left, right;
  size_t op_pos = find_lowest_prec_binary_op(f, pos, len, paren_map);
  switch (f[op_pos]) {
  case 'U':
    tie(lb, ub) = find_bounds(f, op_pos + 1, len - 1, &end_subscript);
    left = parse(f, pos, op_pos - pos, paren_map);
    right = parse(f, end_subscript, end - end_subscript, paren_map);
    return make_unique<Until>(std::move(left), std::move(right), lb, ub);
  case 'R':
    tie(lb, ub) = find_bounds(f, op_pos + 1, len - 1, &end_subscript);
    left = parse(f, pos, op_pos - pos, paren_map);
    right = parse(f, end_subscript, end - end_subscript, paren_map);
    return make_unique<Release>(std::move(left), std::move(right), lb, ub);
  case '&':
    left = parse(f, pos, op_pos - pos, paren_map);
    right = parse(f, op_pos + 1, end - op_pos - 1, paren_map);
    return make_unique<And>(std::move(left), std::move(right));
  case '^':
    left = parse(f, pos, op_pos - pos, paren_map);
    right = parse(f, op_pos + 1, end - op_pos - 1, paren_map);
    return make_unique<Xor>(std::move(left), std::move(right));
  case '|':
    left = parse(f, pos, op_pos - pos, paren_map);
    right = parse(f, op_pos + 1, end - op_pos - 1, paren_map);
    return make_unique<Or>(std::move(left), std::move(right));
  case '-':
    if (pos < op_pos && op_pos + 1 < end && f[op_pos - 1] != '<' &&
        f[op_pos + 1] == '>') {
      left = parse(f, pos, op_pos - pos, paren_map);
      right = parse(f, op_pos + 2, end - op_pos - 2, paren_map);
      return make_unique<Implies>(std::move(left), std::move(right));
    }
    break;
  case '<':
    if (!(op_pos + 2 < end && f[op_pos + 1] == '-' && f[op_pos + 2] == '>')) {
      break;
    }
  case '=':
    left = parse(f, pos, op_pos - pos, paren_map);
    right = parse(f, op_pos + 3, end - op_pos - 3, paren_map);
    return make_unique<Equiv>(std::move(left), std::move(right));
  default:
    break; // not an operator, continue the search
  }
  return nullptr;
}

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

vector<vector<string>>
read_trace_files(const string &trace_directory_path) {
  vector<vector<string>> traces;
  for (const auto &entry : fs::directory_iterator(trace_directory_path)) {
    vector<string> new_trace = read_trace_file(entry.path());
    traces.emplace_back(new_trace);
  }
  return traces;
}

string int_to_bin_str(unsigned int n, int width) {
  string result;
  for (int i = 0; i < width; ++i) {
    result += (n & 1) ? "1" : "0";
    n >>= 1;
  }
  return result;
}

} // namespace libmltl
