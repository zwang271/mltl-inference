#include "quine_mccluskey.h"

#include <algorithm>
#include <assert.h>

/* Takes an integer and returns a string of 0s and 1s corresponding to the
 * binary value of n. The string will be padded so that it has length of width.
 * ex: 14 = "1110"
 */
std::string int_to_bin_str(unsigned int n, int width) {
  std::string result;
  for (int i = 0; i < width; ++i) {
    result += (n & 1) ? "1" : "0";
    n >>= 1;
  }
  return result;
}

/* Returns true if two terms differ by just one bit.
 */
bool is_grey_code(const std::string a, const std::string b) {
  int flag = 0;
  for (int i = 0; i < a.length(); ++i) {
    if (a[i] != b[i])
      ++flag;
  }
  return (flag == 1);
}

/* Replaces complement terms with don't cares
 * ex: 0110 and 0111 becomes 011-
 */
std::string replace_complements(const std::string a, const std::string b) {
  std::string temp = "";
  for (int i = 0; i < a.length(); ++i)
    if (a[i] != b[i])
      temp = temp + "-";
    else
      temp = temp + a[i];

  return temp;
}

/* Returns true if string b exists in vector a.
 */
bool in_vector(const std::vector<std::string> a, const std::string b) {
  for (int i = 0; i < a.size(); ++i)
    if (a[i].compare(b) == 0)
      return true;
  return false;
}

std::vector<std::string> reduce(const std::vector<std::string> minterms) {
  std::vector<std::string> newminterms;

  int max = minterms.size();
  int *checked = new int[max];
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

  delete[] checked;

  return newminterms;
}

/* Returns the string representation of a clause.
 */
std::string get_clause_as_string(const std::string a) {
  const std::string dontcares(a.size(), '-');
  std::string temp = "";
  if (a == dontcares)
    return "true";

  for (int i = 0; i < a.length(); ++i) {
    if (a[i] != '-') {
      if (!temp.empty()) {
        temp += "&";
      }
      if (a[i] == '0') {
        temp = temp + "-p" + std::to_string(i);
      } else {
        temp = temp + "p" + std::to_string(i);
      }
    }
  }
  return temp;
}

/* The Quine-McCluskey method is used to minimize Boolean functions.
 * Takes a vector of strings representing the binary representation of each
 * satisfying assignment.
 * ex: "1011" means that the assignment p0, -p1, p2, p3 evaluates to true.
 */
std::string quine_mccluskey(const std::vector<std::string> *implicants) {
  if (implicants->size() == 0) {
    return "(false)";
  }
  int num_vars = (*implicants)[0].size();
  assert(num_vars > 0);
  for (int i = 1; i < implicants->size(); ++i) {
    assert((*implicants)[i].size() == num_vars);
  }

  std::vector<std::string> minterms = *implicants;
  std::sort(minterms.begin(), minterms.end());
  do {
    minterms = reduce(minterms);
    sort(minterms.begin(), minterms.end());
  } while (minterms != reduce(minterms));

  std::string reduced_dnf;
  int i;
  for (i = 0; i < minterms.size() - 1; ++i) {
    reduced_dnf += "(" + get_clause_as_string(minterms[i]) + ")|";
  }
  reduced_dnf += "(" + get_clause_as_string(minterms[i]) + ")";

  return reduced_dnf;
}
