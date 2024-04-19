#pragma once

#include <memory>
#include <string>
#include <vector>

#define PYBIND

using namespace std;

class ASTNode {
public:
  enum class Type {
    Constant, // true,false
    Variable, // p0,p1,p2,...
    Negation, // ~
    And,      // &
    Xor,      // ^
    Or,       // |
    Implies,  // ->
    Equiv,    // <->
    Finally,  // F
    Globally, // G
    Until,    // U
    Release,  // R
  };

  virtual ASTNode::Type get_type() const = 0;
  virtual string as_string() const = 0;
  /* Evaluates as trace over time steps [begin, end).
   */
  virtual bool evaluate_subt(const vector<string> &trace, size_t begin,
                             size_t end) const = 0;
  bool evaluate(const vector<string> &trace) const {
    return evaluate_subt(trace, 0, trace.size());
  };
  /* Mission-time LTL (MLTL) Formula Validation Via Regular Expressions
   * https://temporallogic.org/research/WEST/WEST_extended.pdf
   * Definition 6
   */
  virtual size_t future_reach() const = 0;
  virtual size_t size() const = 0;
  virtual size_t count(ASTNode::Type target_type) const = 0;
  virtual unique_ptr<ASTNode> deep_copy() const = 0;
  virtual ~ASTNode() = default;
};

class Constant : public ASTNode {
private:
  bool val;

public:
  Constant(bool value) : val(value){};

  bool get_value() const { return val; }

  ASTNode::Type get_type() const { return ASTNode::Type::Constant; }
  string as_string() const { return val ? "true" : "false"; }
  bool evaluate_subt(const vector<string> &trace, size_t begin,
                     size_t end) const {
    return val;
  }
  size_t future_reach() const { return 0; }
  size_t size() const { return 1; }
  size_t count(ASTNode::Type target_type) const {
    return (get_type() == target_type);
  }
  unique_ptr<ASTNode> deep_copy() const { return make_unique<Constant>(val); }
};

class Variable : public ASTNode {
private:
  unsigned int id;

public:
  Variable(unsigned int id) : id(id){};

  unsigned int get_id() const { return id; }

  ASTNode::Type get_type() const { return ASTNode::Type::Variable; }
  string as_string() const { return 'p' + to_string(id); }
  bool evaluate_subt(const vector<string> &trace, size_t begin,
                     size_t end) const {
    if (begin == end || id >= trace[0].length()) {
      return false;
    }
    return (trace[begin][id] == '1');
  }
  size_t future_reach() const { return 1; }
  size_t size() const { return 1; }
  size_t count(ASTNode::Type target_type) const {
    return (get_type() == target_type);
  }
  unique_ptr<ASTNode> deep_copy() const { return make_unique<Variable>(id); }
};

class UnaryOp : public ASTNode {
protected:
  unique_ptr<ASTNode> operand;

public:
  UnaryOp() : operand(nullptr){};
  UnaryOp(unique_ptr<ASTNode> operand) : operand(std::move(operand)){};

  const ASTNode &get_operand() const { return *operand; }
  unique_ptr<ASTNode> release_operand() { return std::move(operand); }
  void set_operand(unique_ptr<ASTNode> new_operand) {
    operand = std::move(new_operand);
  }

  size_t size() const { return 1 + operand->size(); }
  size_t count(ASTNode::Type target_type) const {
    return (get_type() == target_type) + operand->count(target_type);
  }

  virtual ASTNode::Type get_type() const = 0;
  virtual string as_string() const = 0;
  virtual bool evaluate_subt(const vector<string> &trace, size_t begin,
                             size_t end) const = 0;
  virtual size_t future_reach() const = 0;
  virtual unique_ptr<ASTNode> deep_copy() const = 0;
};

class UnaryPropOp : public UnaryOp {
public:
  using UnaryOp::UnaryOp; // inherit base-class constructors

  size_t future_reach() const { return operand->future_reach(); }

  virtual ASTNode::Type get_type() const = 0;
  virtual string as_string() const = 0;
  virtual bool evaluate_subt(const vector<string> &trace, size_t begin,
                             size_t end) const = 0;
  virtual unique_ptr<ASTNode> deep_copy() const = 0;
};

class Negation : public UnaryPropOp {
public:
  using UnaryPropOp::UnaryPropOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::Negation; }
  string as_string() const { return '~' + operand->as_string(); }
  bool evaluate_subt(const vector<string> &trace, size_t begin,
                     size_t end) const {
    return !operand->evaluate_subt(trace, begin, end);
  }
  unique_ptr<ASTNode> deep_copy() const {
    return make_unique<Negation>(operand->deep_copy());
  }
};

class UnaryTempOp : public UnaryOp {
protected:
  size_t lb, ub;

public:
  UnaryTempOp() : UnaryOp(), lb(0), ub(0){};
  UnaryTempOp(unique_ptr<ASTNode> operand, size_t lb, size_t ub)
      : UnaryOp(std::move(operand)), lb(lb), ub(ub){};

  size_t get_lower_bound() const { return lb; }
  size_t get_upper_bound() const { return ub; }
  void set_lower_bound(size_t new_lb) { lb = new_lb; }
  void set_upper_bound(size_t new_ub) { ub = new_ub; }

  size_t future_reach() const { return ub + operand->future_reach(); }

  virtual ASTNode::Type get_type() const = 0;
  virtual string as_string() const = 0;
  virtual bool evaluate_subt(const vector<string> &trace, size_t begin,
                             size_t end) const = 0;
  virtual unique_ptr<ASTNode> deep_copy() const = 0;
};

class Finally : public UnaryTempOp {
public:
  using UnaryTempOp::UnaryTempOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::Finally; }
  string as_string() const {
    return "F[" + to_string(lb) + ',' + to_string(ub) + "](" +
           operand->as_string() + ')';
  }
  bool evaluate_subt(const vector<string> &trace, size_t begin,
                     size_t end) const;
  unique_ptr<ASTNode> deep_copy() const {
    return make_unique<Finally>(operand->deep_copy(), lb, ub);
  }
};

class Globally : public UnaryTempOp {
public:
  using UnaryTempOp::UnaryTempOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::Globally; }
  string as_string() const {
    return "G[" + to_string(lb) + ',' + to_string(ub) + "](" +
           operand->as_string() + ')';
  }
  bool evaluate_subt(const vector<string> &trace, size_t begin,
                     size_t end) const;
  unique_ptr<ASTNode> deep_copy() const {
    return make_unique<Globally>(operand->deep_copy(), lb, ub);
  }
};

class BinaryOp : public ASTNode {
protected:
  unique_ptr<ASTNode> left, right;

public:
  BinaryOp() : left(nullptr), right(nullptr){};
  BinaryOp(unique_ptr<ASTNode> left, unique_ptr<ASTNode> right)
      : left(std::move(left)), right(std::move(right)){};

  const ASTNode &get_left() const { return *left; }
  const ASTNode &get_right() const { return *right; }
  unique_ptr<ASTNode> release_left() { return std::move(left); }
  unique_ptr<ASTNode> release_right() { return std::move(right); }
  void set_left(unique_ptr<ASTNode> new_left) { left = std::move(new_left); }
  void set_right(unique_ptr<ASTNode> new_right) { left = std::move(new_right); }

  size_t size() const { return 1 + left->size() + right->size(); }
  size_t count(ASTNode::Type target_type) const {
    return (get_type() == target_type) + left->count(target_type) +
           right->count(target_type);
  }

  virtual ASTNode::Type get_type() const = 0;
  virtual string as_string() const = 0;
  virtual bool evaluate_subt(const vector<string> &trace, size_t begin,
                             size_t end) const = 0;
  virtual size_t future_reach() const = 0;
  virtual unique_ptr<ASTNode> deep_copy() const = 0;
};

class BinaryPropOp : public BinaryOp {
public:
  using BinaryOp::BinaryOp; // inherit base-class constructor

  size_t future_reach() const {
    return max(left->future_reach(), right->future_reach());
  }

  virtual ASTNode::Type get_type() const = 0;
  virtual string as_string() const = 0;
  virtual bool evaluate_subt(const vector<string> &trace, size_t begin,
                             size_t end) const = 0;
  virtual unique_ptr<ASTNode> deep_copy() const = 0;
};

class And : public BinaryPropOp {
public:
  using BinaryPropOp::BinaryPropOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::And; }
  string as_string() const {
    return '(' + left->as_string() + ")&(" + right->as_string() + ')';
  }
  bool evaluate_subt(const vector<string> &trace, size_t begin,
                     size_t end) const {
    return left->evaluate_subt(trace, begin, end) &&
           right->evaluate_subt(trace, begin, end);
  }
  unique_ptr<ASTNode> deep_copy() const {
    return make_unique<And>(left->deep_copy(), right->deep_copy());
  }
};

class Xor : public BinaryPropOp {
public:
  using BinaryPropOp::BinaryPropOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::Xor; }
  string as_string() const {
    return '(' + left->as_string() + ")^(" + right->as_string() + ')';
  }
  bool evaluate_subt(const vector<string> &trace, size_t begin,
                     size_t end) const {
    return left->evaluate_subt(trace, begin, end) !=
           right->evaluate_subt(trace, begin, end);
  }
  unique_ptr<ASTNode> deep_copy() const {
    return make_unique<And>(left->deep_copy(), right->deep_copy());
  }
};

class Or : public BinaryPropOp {
public:
  using BinaryPropOp::BinaryPropOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::Or; }
  string as_string() const {
    return '(' + left->as_string() + ")|(" + right->as_string() + ')';
  }
  bool evaluate_subt(const vector<string> &trace, size_t begin,
                     size_t end) const {
    return left->evaluate_subt(trace, begin, end) ||
           right->evaluate_subt(trace, begin, end);
  }
  unique_ptr<ASTNode> deep_copy() const {
    return make_unique<Or>(left->deep_copy(), right->deep_copy());
  }
};

class Implies : public BinaryPropOp {
public:
  using BinaryPropOp::BinaryPropOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::Implies; }
  string as_string() const {
    return '(' + left->as_string() + ")->(" + right->as_string() + ')';
  }
  bool evaluate_subt(const vector<string> &trace, size_t begin,
                     size_t end) const {
    return !left->evaluate_subt(trace, begin, end) ||
           right->evaluate_subt(trace, begin, end);
  }
  unique_ptr<ASTNode> deep_copy() const {
    return make_unique<Implies>(left->deep_copy(), right->deep_copy());
  }
};

class Equiv : public BinaryPropOp {
public:
  using BinaryPropOp::BinaryPropOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::Equiv; }
  string as_string() const {
    return '(' + left->as_string() + ")<->(" + right->as_string() + ')';
  }
  bool evaluate_subt(const vector<string> &trace, size_t begin,
                     size_t end) const {
    return left->evaluate_subt(trace, begin, end) ==
           right->evaluate_subt(trace, begin, end);
  }
  unique_ptr<ASTNode> deep_copy() const {
    return make_unique<Equiv>(left->deep_copy(), right->deep_copy());
  }
};

class BinaryTempOp : public BinaryOp {
protected:
  size_t lb, ub;

public:
  BinaryTempOp() : BinaryOp(), lb(0), ub(0){};
  BinaryTempOp(unique_ptr<ASTNode> left, unique_ptr<ASTNode> right, size_t lb,
               size_t ub)
      : BinaryOp(std::move(left), std::move(right)), lb(lb), ub(ub){};

  size_t get_lower_bound() const { return lb; }
  size_t get_upper_bound() const { return ub; }
  void set_lower_bound(size_t new_lb) { lb = new_lb; }
  void set_upper_bound(size_t new_ub) { ub = new_ub; }

  size_t future_reach() const {
    // need to be careful here to avoid an underflow when subtracting 1
    size_t lfr = left->future_reach();
    size_t rfr = right->future_reach();
    if (lfr > rfr) {
      return ub + lfr - 1;
    }
    return ub + rfr;
  }

  virtual ASTNode::Type get_type() const = 0;
  virtual string as_string() const = 0;
  virtual bool evaluate_subt(const vector<string> &trace, size_t begin,
                             size_t end) const = 0;
  virtual unique_ptr<ASTNode> deep_copy() const = 0;
};

class Until : public BinaryTempOp {
public:
  using BinaryTempOp::BinaryTempOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::Until; }
  string as_string() const {
    return '(' + left->as_string() + ")U[" + to_string(lb) + ',' +
           to_string(ub) + "](" + right->as_string() + ')';
  }
  bool evaluate_subt(const vector<string> &trace, size_t begin,
                     size_t end) const;
  unique_ptr<ASTNode> deep_copy() const {
    return make_unique<Until>(left->deep_copy(), right->deep_copy(), lb, ub);
  }
};

class Release : public BinaryTempOp {
public:
  using BinaryTempOp::BinaryTempOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::Release; }
  string as_string() const {
    return '(' + left->as_string() + ")R[" + to_string(lb) + ',' +
           to_string(ub) + "](" + right->as_string() + ')';
  }
  bool evaluate_subt(const vector<string> &trace, size_t begin,
                     size_t end) const;
  unique_ptr<ASTNode> deep_copy() const {
    return make_unique<Release>(left->deep_copy(), right->deep_copy(), lb, ub);
  }
};
