#pragma once

#include "ast.hh"

std::string
quine_mccluskey_fast_string(const std::vector<std::string> &implicants);
std::unique_ptr<libmltl::ASTNode>
quine_mccluskey(const std::vector<std::string> &implicants);
