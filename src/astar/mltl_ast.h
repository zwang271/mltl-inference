#pragma once

#include <memory>
#include <string>
#include <vector>

using namespace std;

enum class MLTLNodeType {
  PropCons,
  PropVar,
  UnaryPropOp,
  BinaryPropOp,
  UnaryTempOp,
  BinaryTempOp,
};

enum class MLTLUnaryPropOpType {
  Neg,
};

enum class MLTLBinaryPropOpType {
  And, // highest precedence
  Xor,
  Or,
  Implies,
  Equiv, // lowest precedence
};

enum class MLTLUnaryTempOpType {
  Finally,
  Globally,
};

enum class MLTLBinaryTempOpType {
  Until,
  Release,
};

class MLTLNode {
private:
  MLTLNodeType type;

protected:
  MLTLNode(MLTLNodeType type);

public:
  virtual ~MLTLNode(){};
  MLTLNodeType getType() const;
  virtual string as_string() const = 0;
  virtual unsigned int future_reach() const = 0;
  virtual bool evaluate(vector<string> &trace) const = 0;
};

class MLTLPropConsNode : public MLTLNode {
public:
  bool val;

  MLTLPropConsNode(bool val);

  string as_string() const;
  unsigned int future_reach() const;
  bool evaluate(vector<string> &trace) const;
};

class MLTLPropVarNode : public MLTLNode {
public:
  unsigned int var_id;

  MLTLPropVarNode(unsigned int var_id);

  string as_string() const;
  unsigned int future_reach() const;
  bool evaluate(vector<string> &trace) const;
};

class MLTLUnaryPropOpNode : public MLTLNode {
public:
  MLTLUnaryPropOpType op_type;
  unique_ptr<MLTLNode> child;

  MLTLUnaryPropOpNode(MLTLUnaryPropOpType op_type, unique_ptr<MLTLNode> child);

  string as_string() const;
  unsigned int future_reach() const;
  bool evaluate(vector<string> &trace) const;
};

class MLTLBinaryPropOpNode : public MLTLNode {
public:
  MLTLBinaryPropOpType op_type;
  unique_ptr<MLTLNode> left;
  unique_ptr<MLTLNode> right;

  MLTLBinaryPropOpNode(MLTLBinaryPropOpType op_type, unique_ptr<MLTLNode> left,
                       unique_ptr<MLTLNode> right);

  string as_string() const;
  unsigned int future_reach() const;
  bool evaluate(vector<string> &trace) const;
};

class MLTLUnaryTempOpNode : public MLTLNode {
public:
  MLTLUnaryTempOpType op_type;
  unsigned int lb;
  unsigned int ub;
  unique_ptr<MLTLNode> child;

  MLTLUnaryTempOpNode(MLTLUnaryTempOpType op_type, unsigned int lb,
                      unsigned int ub, unique_ptr<MLTLNode> child);

  string as_string() const;
  unsigned int future_reach() const;
  bool evaluate(vector<string> &trace) const;
};

class MLTLBinaryTempOpNode : public MLTLNode {
public:
  MLTLBinaryTempOpType op_type;
  unsigned int lb;
  unsigned int ub;
  unique_ptr<MLTLNode> left;
  unique_ptr<MLTLNode> right;

  MLTLBinaryTempOpNode(MLTLBinaryTempOpType op_type, unsigned int lb,
                       unsigned int ub, unique_ptr<MLTLNode> left,
                       unique_ptr<MLTLNode> right);

  string as_string() const;
  unsigned int future_reach() const;
  bool evaluate(vector<string> &trace) const;
};
