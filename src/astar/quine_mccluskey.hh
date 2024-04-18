#pragma once

#include "libmltl/ast.hh"

using namespace std;

string int_to_bin_str(unsigned int n, int width);
string quine_mccluskey_fast_string(const vector<string> &implicants);
unique_ptr<ASTNode> quine_mccluskey(const vector<string> &implicants);
