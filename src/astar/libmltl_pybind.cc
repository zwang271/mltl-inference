#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "libmltl.hh"

namespace py = pybind11;

PYBIND11_MODULE(libmltl, m) {
  // Bind enums
  py::enum_<MLTLNodeType>(m, "MLTLNodeType")
      .value("PropCons", MLTLNodeType::PropCons)
      .value("PropVar", MLTLNodeType::PropVar)
      .value("UnaryPropOp", MLTLNodeType::UnaryPropOp)
      .value("BinaryPropOp", MLTLNodeType::BinaryPropOp)
      .value("UnaryTempOp", MLTLNodeType::UnaryTempOp)
      .value("BinaryTempOp", MLTLNodeType::BinaryTempOp)
      .export_values();

  py::enum_<MLTLUnaryPropOpType>(m, "MLTLUnaryPropOpType")
      .value("Neg", MLTLUnaryPropOpType::Neg)
      .export_values();

  py::enum_<MLTLBinaryPropOpType>(m, "MLTLBinaryPropOpType")
      .value("And", MLTLBinaryPropOpType::And)
      .value("Xor", MLTLBinaryPropOpType::Xor)
      .value("Or", MLTLBinaryPropOpType::Or)
      .value("Implies", MLTLBinaryPropOpType::Implies)
      .value("Equiv", MLTLBinaryPropOpType::Equiv)
      .export_values();

  py::enum_<MLTLUnaryTempOpType>(m, "MLTLUnaryTempOpType")
      .value("Finally", MLTLUnaryTempOpType::Finally)
      .value("Globally", MLTLUnaryTempOpType::Globally)
      .export_values();

  py::enum_<MLTLBinaryTempOpType>(m, "MLTLBinaryTempOpType")
      .value("Until", MLTLBinaryTempOpType::Until)
      .value("Release", MLTLBinaryTempOpType::Release)
      .export_values();

  m.def("parse", &parse, py::arg("f"));

  py::class_<MLTLNode, std::unique_ptr<MLTLNode>>(m, "MLTLNode");

  py::class_<MLTLPropConsNode, MLTLNode>(m, "MLTLPropConsNode")
      .def(py::init<bool>())
      .def("as_string", &MLTLPropConsNode::as_string)
      .def("future_reach", &MLTLPropConsNode::future_reach)
      .def("evaluate", &MLTLPropConsNode::evaluate)
      .def("size", &MLTLPropConsNode::size)
      .def("count", (size_t(MLTLPropConsNode::*)(MLTLNodeType) const) &
                        MLTLPropConsNode::count)
      .def("count", (size_t(MLTLPropConsNode::*)(MLTLUnaryPropOpType) const) &
                        MLTLPropConsNode::count)
      .def("count", (size_t(MLTLPropConsNode::*)(MLTLBinaryPropOpType) const) &
                        MLTLPropConsNode::count)
      .def("count", (size_t(MLTLPropConsNode::*)(MLTLUnaryTempOpType) const) &
                        MLTLPropConsNode::count)
      .def("count", (size_t(MLTLPropConsNode::*)(MLTLBinaryTempOpType) const) &
                        MLTLPropConsNode::count);

  py::class_<MLTLPropVarNode, MLTLNode>(m, "MLTLPropVarNode")
      .def(py::init<size_t>())
      .def("as_string", &MLTLPropVarNode::as_string)
      .def("future_reach", &MLTLPropVarNode::future_reach)
      .def("evaluate", &MLTLPropVarNode::evaluate)
      .def("size", &MLTLPropVarNode::size)
      .def("count", (size_t(MLTLPropVarNode::*)(MLTLNodeType) const) &
                        MLTLPropVarNode::count)
      .def("count", (size_t(MLTLPropVarNode::*)(MLTLUnaryPropOpType) const) &
                        MLTLPropVarNode::count)
      .def("count", (size_t(MLTLPropVarNode::*)(MLTLBinaryPropOpType) const) &
                        MLTLPropVarNode::count)
      .def("count", (size_t(MLTLPropVarNode::*)(MLTLUnaryTempOpType) const) &
                        MLTLPropVarNode::count)
      .def("count", (size_t(MLTLPropVarNode::*)(MLTLBinaryTempOpType) const) &
                        MLTLPropVarNode::count);

  py::class_<MLTLUnaryPropOpNode, MLTLNode>(m, "MLTLUnaryPropOpNode")
      .def(py::init<MLTLUnaryPropOpType, MLTLNode *>())
      .def("as_string", &MLTLUnaryPropOpNode::as_string)
      .def("future_reach", &MLTLUnaryPropOpNode::future_reach)
      .def("evaluate", &MLTLUnaryPropOpNode::evaluate)
      .def("size", &MLTLUnaryPropOpNode::size)
      .def("count", (size_t(MLTLUnaryPropOpNode::*)(MLTLNodeType) const) &
                        MLTLUnaryPropOpNode::count)
      .def("count",
           (size_t(MLTLUnaryPropOpNode::*)(MLTLUnaryPropOpType) const) &
               MLTLUnaryPropOpNode::count)
      .def("count",
           (size_t(MLTLUnaryPropOpNode::*)(MLTLBinaryPropOpType) const) &
               MLTLUnaryPropOpNode::count)
      .def("count",
           (size_t(MLTLUnaryPropOpNode::*)(MLTLUnaryTempOpType) const) &
               MLTLUnaryPropOpNode::count)
      .def("count",
           (size_t(MLTLUnaryPropOpNode::*)(MLTLBinaryTempOpType) const) &
               MLTLUnaryPropOpNode::count);

  py::class_<MLTLBinaryPropOpNode, MLTLNode>(m, "MLTLBinaryPropOpNode")
      .def(py::init<MLTLBinaryPropOpType, MLTLNode *, MLTLNode *>())
      .def("as_string", &MLTLBinaryPropOpNode::as_string)
      .def("future_reach", &MLTLBinaryPropOpNode::future_reach)
      .def("evaluate", &MLTLBinaryPropOpNode::evaluate)
      .def("size", &MLTLBinaryPropOpNode::size)
      .def("count", (size_t(MLTLBinaryPropOpNode::*)(MLTLNodeType) const) &
                        MLTLBinaryPropOpNode::count)
      .def("count",
           (size_t(MLTLBinaryPropOpNode::*)(MLTLUnaryPropOpType) const) &
               MLTLBinaryPropOpNode::count)
      .def("count",
           (size_t(MLTLBinaryPropOpNode::*)(MLTLBinaryPropOpType) const) &
               MLTLBinaryPropOpNode::count)
      .def("count",
           (size_t(MLTLBinaryPropOpNode::*)(MLTLUnaryTempOpType) const) &
               MLTLBinaryPropOpNode::count)
      .def("count",
           (size_t(MLTLBinaryPropOpNode::*)(MLTLBinaryTempOpType) const) &
               MLTLBinaryPropOpNode::count);

  py::class_<MLTLUnaryTempOpNode, MLTLNode>(m, "MLTLUnaryTempOpNode")
      .def(py::init<MLTLUnaryTempOpType, size_t, size_t, MLTLNode *>())
      .def("as_string", &MLTLUnaryTempOpNode::as_string)
      .def("future_reach", &MLTLUnaryTempOpNode::future_reach)
      .def("evaluate", &MLTLUnaryTempOpNode::evaluate)
      .def("size", &MLTLUnaryTempOpNode::size)
      .def("count", (size_t(MLTLUnaryTempOpNode::*)(MLTLNodeType) const) &
                        MLTLUnaryTempOpNode::count)
      .def("count",
           (size_t(MLTLUnaryTempOpNode::*)(MLTLUnaryPropOpType) const) &
               MLTLUnaryTempOpNode::count)
      .def("count",
           (size_t(MLTLUnaryTempOpNode::*)(MLTLBinaryPropOpType) const) &
               MLTLUnaryTempOpNode::count)
      .def("count",
           (size_t(MLTLUnaryTempOpNode::*)(MLTLUnaryTempOpType) const) &
               MLTLUnaryTempOpNode::count)
      .def("count",
           (size_t(MLTLUnaryTempOpNode::*)(MLTLBinaryTempOpType) const) &
               MLTLUnaryTempOpNode::count);

  py::class_<MLTLBinaryTempOpNode, MLTLNode>(m, "MLTLBinaryTempOpNode")
      .def(py::init<MLTLBinaryTempOpType, size_t, size_t, MLTLNode *,
                    MLTLNode *>())
      .def("as_string", &MLTLBinaryTempOpNode::as_string)
      .def("future_reach", &MLTLBinaryTempOpNode::future_reach)
      .def("evaluate", &MLTLBinaryTempOpNode::evaluate)
      .def("size", &MLTLBinaryTempOpNode::size)
      .def("count", (size_t(MLTLBinaryTempOpNode::*)(MLTLNodeType) const) &
                        MLTLBinaryTempOpNode::count)
      .def("count",
           (size_t(MLTLBinaryTempOpNode::*)(MLTLUnaryPropOpType) const) &
               MLTLBinaryTempOpNode::count)
      .def("count",
           (size_t(MLTLBinaryTempOpNode::*)(MLTLBinaryPropOpType) const) &
               MLTLBinaryTempOpNode::count)
      .def("count",
           (size_t(MLTLBinaryTempOpNode::*)(MLTLUnaryTempOpType) const) &
               MLTLBinaryTempOpNode::count)
      .def("count",
           (size_t(MLTLBinaryTempOpNode::*)(MLTLBinaryTempOpType) const) &
               MLTLBinaryTempOpNode::count);
}
