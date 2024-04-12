#pragma once
#include <memory>
#include <string>
#include <vector>

#include "mltl_ast.h"

using namespace std;

string int_to_bin_str(unsigned int n, int width);
string quine_mccluskey_fast_string(const vector<string> *implicants);
unique_ptr<MLTLNode> quine_mccluskey(const vector<string> *implicants);
