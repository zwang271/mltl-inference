#pragma once

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

class MLTLNode;

MLTLNode *parse(const string &f);

class MLTLNode {
protected:
  const MLTLNodeType type;
  MLTLNode(MLTLNodeType type);

public:
  MLTLNodeType getType() const;

  virtual ~MLTLNode() = 0;
  virtual string as_string() const = 0;
  virtual size_t future_reach() const = 0;
  virtual bool evaluate(vector<string> &trace) const = 0;
  virtual size_t size() const = 0;
  virtual size_t count(MLTLNodeType target_type) const = 0;
  virtual size_t count(MLTLUnaryPropOpType target_type) const = 0;
  virtual size_t count(MLTLBinaryPropOpType target_type) const = 0;
  virtual size_t count(MLTLUnaryTempOpType target_type) const = 0;
  virtual size_t count(MLTLBinaryTempOpType target_type) const = 0;
};

class MLTLPropConsNode : public MLTLNode {
public:
  bool val;

  MLTLPropConsNode(bool val);
  ~MLTLPropConsNode();

  string as_string() const;
  size_t future_reach() const;
  bool evaluate(vector<string> &trace) const;
  size_t size() const;
  size_t count(MLTLNodeType target_type) const;
  size_t count(MLTLUnaryPropOpType target_type) const;
  size_t count(MLTLBinaryPropOpType target_type) const;
  size_t count(MLTLUnaryTempOpType target_type) const;
  size_t count(MLTLBinaryTempOpType target_type) const;
};

class MLTLPropVarNode : public MLTLNode {
public:
  size_t var_id;

  MLTLPropVarNode(unsigned int var_id);
  ~MLTLPropVarNode();

  string as_string() const;
  size_t future_reach() const;
  bool evaluate(vector<string> &trace) const;
  size_t size() const;
  size_t count(MLTLNodeType target_type) const;
  size_t count(MLTLUnaryPropOpType target_type) const;
  size_t count(MLTLBinaryPropOpType target_type) const;
  size_t count(MLTLUnaryTempOpType target_type) const;
  size_t count(MLTLBinaryTempOpType target_type) const;
};

class MLTLUnaryPropOpNode : public MLTLNode {
public:
  MLTLUnaryPropOpType op_type;
  MLTLNode *operand;

  MLTLUnaryPropOpNode(MLTLUnaryPropOpType op_type, MLTLNode *operand);
  ~MLTLUnaryPropOpNode();

  string as_string() const;
  size_t future_reach() const;
  bool evaluate(vector<string> &trace) const;
  size_t size() const;
  size_t count(MLTLNodeType target_type) const;
  size_t count(MLTLUnaryPropOpType target_type) const;
  size_t count(MLTLBinaryPropOpType target_type) const;
  size_t count(MLTLUnaryTempOpType target_type) const;
  size_t count(MLTLBinaryTempOpType target_type) const;
};

class MLTLBinaryPropOpNode : public MLTLNode {
public:
  MLTLBinaryPropOpType op_type;
  MLTLNode *left;
  MLTLNode *right;

  MLTLBinaryPropOpNode(MLTLBinaryPropOpType op_type, MLTLNode *left,
                       MLTLNode *right);
  ~MLTLBinaryPropOpNode();

  string as_string() const;
  size_t future_reach() const;
  bool evaluate(vector<string> &trace) const;
  size_t size() const;
  size_t count(MLTLNodeType target_type) const;
  size_t count(MLTLUnaryPropOpType target_type) const;
  size_t count(MLTLBinaryPropOpType target_type) const;
  size_t count(MLTLUnaryTempOpType target_type) const;
  size_t count(MLTLBinaryTempOpType target_type) const;
};

class MLTLUnaryTempOpNode : public MLTLNode {
public:
  MLTLUnaryTempOpType op_type;
  size_t lb;
  size_t ub;
  MLTLNode *operand;

  MLTLUnaryTempOpNode(MLTLUnaryTempOpType op_type, size_t lb, size_t ub,
                      MLTLNode *operand);
  ~MLTLUnaryTempOpNode();

  string as_string() const;
  size_t future_reach() const;
  bool evaluate(vector<string> &trace) const;
  size_t size() const;
  size_t count(MLTLNodeType target_type) const;
  size_t count(MLTLUnaryPropOpType target_type) const;
  size_t count(MLTLBinaryPropOpType target_type) const;
  size_t count(MLTLUnaryTempOpType target_type) const;
  size_t count(MLTLBinaryTempOpType target_type) const;
};

class MLTLBinaryTempOpNode : public MLTLNode {
public:
  MLTLBinaryTempOpType op_type;
  size_t lb;
  size_t ub;
  MLTLNode *left;
  MLTLNode *right;

  MLTLBinaryTempOpNode(MLTLBinaryTempOpType op_type, size_t lb, size_t ub,
                       MLTLNode *left, MLTLNode *right);
  ~MLTLBinaryTempOpNode();

  string as_string() const;
  size_t future_reach() const;
  bool evaluate(vector<string> &trace) const;
  size_t size() const;
  size_t count(MLTLNodeType target_type) const;
  size_t count(MLTLUnaryPropOpType target_type) const;
  size_t count(MLTLBinaryPropOpType target_type) const;
  size_t count(MLTLUnaryTempOpType target_type) const;
  size_t count(MLTLBinaryTempOpType target_type) const;
};
