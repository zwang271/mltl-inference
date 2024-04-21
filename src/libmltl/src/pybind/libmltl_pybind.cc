#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "parser.hh"

/* Some constructor bindings are missing since unique_ptr has no representation
 * in Python. However default constructors can still be used. Please not that
 * the set_operand/set_left/set_right functions have been wrapped since Python
 * does not support unique_ptr this requires deep copies to be made when using
 * these operations which incurs additional runtime cost.
 *
 *
 * From pybind11 docs:
 *   void do_something_with_example(std::unique_ptr<Example> ex) { ... }
 *
 *   The above signature would imply that Python needs to give up ownership of
 *   an object that is passed to this function, which is generally not possible
 *   (for instance, the object might be referenced elsewhere).
 *
 *   https://pybind11.readthedocs.io/en/stable/advanced/smart_ptrs.html#std-unique-ptr
 */

namespace py = pybind11;
using namespace libmltl;

PYBIND11_MODULE(libmltl, m) {
  /* ast.hh
   */
  py::class_<ASTNode>(m, "ASTNode")
      .def("get_type", &ASTNode::get_type)
      .def("as_string", &ASTNode::as_string)
      .def("evaluate", &ASTNode::evaluate)
      .def("future_reach", &ASTNode::future_reach)
      .def("size", &ASTNode::size)
      .def("depth", &ASTNode::depth)
      .def("count", &ASTNode::count)
      .def("deep_copy", &ASTNode::deep_copy)
      .def(py::self == py::self)
      .def(py::self != py::self)
      .def(py::self < py::self)
      .def(py::self > py::self)
      .def(py::self <= py::self)
      .def(py::self >= py::self);

  py::enum_<ASTNode::Type>(m, "Type")
      .value("Constant", ASTNode::Type::Constant)
      .value("Variable", ASTNode::Type::Variable)
      .value("Negation", ASTNode::Type::Negation)
      .value("And", ASTNode::Type::And)
      .value("Xor", ASTNode::Type::Xor)
      .value("Or", ASTNode::Type::Or)
      .value("Implies", ASTNode::Type::Implies)
      .value("Equiv", ASTNode::Type::Equiv)
      .value("Finally", ASTNode::Type::Finally)
      .value("Globally", ASTNode::Type::Globally)
      .value("Until", ASTNode::Type::Until)
      .value("Release", ASTNode::Type::Release);

  py::class_<Constant, ASTNode>(m, "Constant")
      .def(py::init<bool>())
      .def("get_value", &Constant::get_value);

  py::class_<Variable, ASTNode>(m, "Variable")
      .def(py::init<unsigned int>())
      .def("get_id", &Variable::get_id);

  py::class_<UnaryOp, ASTNode>(m, "UnaryOp")
      .def("get_operand", &UnaryOp::get_operand,
           py::return_value_policy::reference)
      .def("release_operand", &UnaryOp::release_operand)
      .def("set_operand",
           // python has no mechanism pass unique_ptr
           [](UnaryOp &self, ASTNode &new_operand) {
             return self.set_operand(new_operand.deep_copy());
           });

  py::class_<UnaryPropOp, UnaryOp>(m, "UnaryPropOp");

  py::class_<Negation, UnaryPropOp>(m, "Negation").def(py::init<>());
  // .def(py::init<unique_ptr<ASTNode>>())

  py::class_<UnaryTempOp, UnaryOp>(m, "UnaryTempOp")
      .def("get_lower_bound", &UnaryTempOp::get_lower_bound)
      .def("get_upper_bound", &UnaryTempOp::get_upper_bound)
      .def("set_lower_bound", &UnaryTempOp::set_lower_bound)
      .def("set_upper_bound", &UnaryTempOp::set_upper_bound);

  py::class_<Finally, UnaryTempOp>(m, "Finally")
      // .def(py::init<unique_ptr<ASTNode>, size_t, size_t>())
      .def(py::init<>());

  py::class_<Globally, UnaryTempOp>(m, "Globally").def(py::init<>());
  // .def(py::init<unique_ptr<ASTNode>, size_t, size_t>())

  py::class_<BinaryOp, ASTNode>(m, "BinaryOp")
      .def("get_left", &BinaryOp::get_left, py::return_value_policy::reference)
      .def("get_right", &BinaryOp::get_right,
           py::return_value_policy::reference)
      .def("release_left", &BinaryOp::release_left)
      .def("release_right", &BinaryOp::release_right)
      .def("set_left",
           // python has no mechanism pass unique_ptr
           [](BinaryOp &self, ASTNode &new_left) {
             return self.set_left(new_left.deep_copy());
           })
      .def("set_right",
           // python has no mechanism pass unique_ptr
           [](BinaryOp &self, ASTNode &new_right) {
             return self.set_right(new_right.deep_copy());
           });

  py::class_<BinaryPropOp, BinaryOp>(m, "BinaryPropOp");

  py::class_<And, BinaryPropOp>(m, "And")
      .def(py::init<>());
      // .def(py::init<unique_ptr<ASTNode>, unique_ptr<ASTNode>>())

  py::class_<Xor, BinaryPropOp>(m, "Xor")
      .def(py::init<>());
      // .def(py::init<unique_ptr<ASTNode>, unique_ptr<ASTNode>>())

  py::class_<Or, BinaryPropOp>(m, "Or")
      .def(py::init<>());
      // .def(py::init<unique_ptr<ASTNode>, unique_ptr<ASTNode>>())

  py::class_<Implies, BinaryPropOp>(m, "Implies")
      .def(py::init<>());
      // .def(py::init<unique_ptr<ASTNode>, unique_ptr<ASTNode>>())

  py::class_<Equiv, BinaryPropOp>(m, "Equiv")
      .def(py::init<>());
      // .def(py::init<unique_ptr<ASTNode>, unique_ptr<ASTNode>>())

  py::class_<BinaryTempOp, BinaryOp>(m, "BinaryTempOp")
      .def("get_lower_bound", &BinaryTempOp::get_lower_bound)
      .def("get_upper_bound", &BinaryTempOp::get_upper_bound)
      .def("set_lower_bound", &BinaryTempOp::set_lower_bound)
      .def("set_upper_bound", &BinaryTempOp::set_upper_bound);

  py::class_<Until, BinaryTempOp>(m, "Until")
      .def(py::init<>());
      // .def(py::init<unique_ptr<ASTNode>, unique_ptr<ASTNode>, size_t,
      // size_t>())

  py::class_<Release, BinaryTempOp>(m, "Release")
      .def(py::init<>());
      // .def(py::init<unique_ptr<ASTNode>, unique_ptr<ASTNode>, size_t,
      // size_t>())

  /* parser.hh
   */
  m.def("parse", &parse);
  m.def("read_trace_file", &read_trace_file);
  m.def("read_trace_files", &read_trace_files);
  m.def("int_to_bin_str", &int_to_bin_str);
}
