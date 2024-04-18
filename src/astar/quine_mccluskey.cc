#include "quine_mccluskey.hh"

#include <algorithm>
#include <assert.h>

/* Returns true if two terms differ by just one bit.
 */
bool is_grey_code(const string &a, const string &b) {
  int flag = 0;
  for (size_t i = 0; i < a.length(); ++i) {
    if (a[i] != b[i])
      ++flag;
  }
  return (flag == 1);
}

/* Replaces complement terms with don't cares
 * ex: 0110 and 0111 becomes 011-
 */
string replace_complements(const string &a, const string &b) {
  string temp = "";
  for (size_t i = 0; i < a.length(); ++i)
    if (a[i] != b[i])
      temp = temp + "-";
    else
      temp = temp + a[i];

  return temp;
}

/* Returns true if string b exists in vector a.
 */
bool in_vector(const vector<string> &a, const string &b) {
  for (size_t i = 0; i < a.size(); ++i)
    if (a[i].compare(b) == 0)
      return true;
  return false;
}

vector<string> reduce(const vector<string> &minterms) {
  vector<string> newminterms;

  int max = minterms.size();
  vector<int> checked(max);
  for (int i = 0; i < max; ++i) {
    for (int j = i; j < max; ++j) {
      // If a gray code pair is found, replace the differing bits with don't
      // cares.
      if (is_grey_code(minterms[i], minterms[j])) {
        checked[i] = 1;
        checked[j] = 1;
        if (!in_vector(newminterms,
                       replace_complements(minterms[i], minterms[j])))
          newminterms.push_back(replace_complements(minterms[i], minterms[j]));
      }
    }
  }

  // appending all reduced terms to a new vector
  for (int i = 0; i < max; ++i) {
    if (checked[i] != 1 && !in_vector(newminterms, minterms[i]))
      newminterms.push_back(minterms[i]);
  }

  return newminterms;
}

/* Returns the string representation of a clause.
 */
string get_clause_as_string(const string &a) {
  const string dontcares(a.size(), '-');
  string temp = "";
  if (a == dontcares)
    return "true";

  int vars = 0;
  for (size_t i = 0; i < a.length(); ++i) {
    if (a[i] != '-') {
      ++vars;
      if (vars > 1) {
        // temp += "&";
        temp += "&("; // INVALID FORMULA BUG WORK AROUND
      }
      if (a[i] == '0') {
        temp = temp + "~p" + to_string(i);
      } else {
        temp = temp + "p" + to_string(i);
      }
    }
  }
  if (vars > 1) {
    temp = "(" + temp + ")";
    temp.append(vars - 1, ')'); // INVALID FORMULA BUG WORK AROUND
  }
  return temp;
}

/* Returns the ast representation of a clause.
 */
unique_ptr<ASTNode> get_clause_as_ast(const string &a) {
  const string dontcares(a.size(), '-');
  string temp = "";
  if (a == dontcares)
    return make_unique<Constant>(true);

  vector<unique_ptr<ASTNode>> literals;

  for (size_t i = 0; i < a.length(); ++i) {
    if (a[i] != '-') {
      if (a[i] == '0') {
        literals.emplace_back(make_unique<Negation>(
            make_unique<Variable>((unsigned int)i)));
      } else {
        literals.emplace_back(make_unique<Variable>((unsigned int)i));
      }
    }
  }

  // build ast from literals
  unique_ptr<ASTNode> root_node = std::move(literals.back());
  for (int i = (int)literals.size() - 2; i >= 0; --i) {
    root_node = make_unique<And>(std::move(literals[i]), std::move(root_node));
  }

  return root_node;
}

/* The Quine-McCluskey method is used to minimize Boolean functions.
 * Takes a vector of strings representing the binary representation of each
 * satisfying assignment.
 * ex: "1011" means that the assignment p0, -p1, p2, p3 evaluates to true.
 *
 * This version of the function directly returns a string, which is faster than
 * building an AST then calling as_string.
 */
string quine_mccluskey_fast_string(const vector<string> &implicants) {
  if (implicants.size() == 0) {
    return "false";
  }

#ifndef NDEBUG
  size_t num_vars = implicants[0].length();
  assert(num_vars > 0);
  for (size_t i = 1; i < implicants.size(); ++i) {
    assert(implicants[i].size() == num_vars);
  }
#endif

  vector<string> minterms = implicants;
  sort(minterms.begin(), minterms.end());
  do {
    minterms = reduce(minterms);
    sort(minterms.begin(), minterms.end());
  } while (minterms != reduce(minterms));

  string reduced_dnf;
  size_t i;
  for (i = 0; i < minterms.size() - 1; ++i) {
    // reduced_dnf += get_clause_as_string(minterms[i]) + "|";
    reduced_dnf += get_clause_as_string(minterms[i]) +
                   "|("; // INVALID FORMULA BUG WORK AROUND
  }
  reduced_dnf += get_clause_as_string(minterms[i]);
  reduced_dnf.append(minterms.size() - 1,
                     ')'); // INVALID FORMULA BUG WORK AROUND
  if (minterms.size() == 1 && reduced_dnf[0] == '(' &&
      reduced_dnf.back() == ')') {
    // trim redundant parens
    reduced_dnf = reduced_dnf.substr(1, reduced_dnf.length() - 2);
  }

  return reduced_dnf;
}

/* The Quine-McCluskey method is used to minimize Boolean functions.
 * Takes a vector of strings representing the binary representation of each
 * satisfying assignment.
 * ex: "1011" means that the assignment p0, -p1, p2, p3 evaluates to true.
 */
unique_ptr<ASTNode> quine_mccluskey(const vector<string> &implicants) {
  if (implicants.size() == 0) {
    return make_unique<Constant>(false);
  }

#ifndef NDEBUG
  size_t num_vars = implicants[0].length();
  assert(num_vars > 0);
  for (size_t i = 1; i < implicants.size(); ++i) {
    assert(implicants[i].size() == num_vars);
  }
#endif

  vector<string> minterms = implicants;
  sort(minterms.begin(), minterms.end());
  do {
    minterms = reduce(minterms);
    sort(minterms.begin(), minterms.end());
  } while (minterms != reduce(minterms));

  unique_ptr<ASTNode> root_node = get_clause_as_ast(minterms.back());
  for (int i = (int)minterms.size() - 2; i >= 0; --i) {
    unique_ptr<ASTNode> new_clause = get_clause_as_ast(minterms[i]);
    root_node = make_unique<Or>(std::move(new_clause), std::move(root_node));
  }

  return root_node;
}
