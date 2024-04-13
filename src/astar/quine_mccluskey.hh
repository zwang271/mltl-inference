#pragma once
#include <memory>
#include <string>
#include <vector>

#include "libmltl.hh"

using namespace std;

string int_to_bin_str(unsigned int n, int width);
string quine_mccluskey_fast_string(const vector<string> &implicants);
MLTLNode *quine_mccluskey(const vector<string> &implicants);
