#pragma once

#include "ast.hh"

namespace libmltl {

class syntax_error : public std::exception {
private:
  const std::string message;

public:
  syntax_error(const std::string &message) : message(message) {}
  const char *what() const throw() { return message.c_str(); }
};

/* Parses a string representing an MLTL formula. Returns a pointer to the root
 * node of the AST representation. 
 *
 * On an invalid MLTL formula, program will exit and print syntax error.
 */
std::unique_ptr<ASTNode> parse(const std::string &formula);

/* Reads a trace from file.
 *
 * Expected file format:
 *   Each line consists of comma separated 0/1's representing the truth value of
 *   p0,p1,p2,...,pi. The state at the first time step of the trace is the first
 *   line, the second time step is the second line, and so on.
 *
 *   ex:
 *     0,1,1
 *     1,1,1
 *     0,1,0
 *     0,1,1
 *     0,0,0
 *
 *     Returns {"011","111","010","011"."000"}
 */
std::vector<std::string> read_trace_file(const std::string &trace_file_path);

/* Reads all files in a directory and parses them as traces.
 */
std::vector<std::vector<std::string>>
read_trace_files(const std::string &trace_directory_path);

/* Takes an integer and returns a string of 0s and 1s corresponding to the
 * binary value of n. The string will be zero left-padded or truncated to
 * length. Useful function for generating traces.
 *
 * ex:
 *   int_to_bin_str(11, 5) == "01011"
 */
std::string int_to_bin_str(unsigned int n, int width);

} // namespace libmltl
