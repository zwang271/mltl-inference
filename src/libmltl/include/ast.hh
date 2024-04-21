#pragma once

#include <memory>
#include <string>
#include <vector>

namespace libmltl {

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
  virtual std::string as_string() const = 0;
  /* Evaluates as trace over time steps [begin, end).
   */
  virtual bool evaluate_subt(const std::vector<std::string> &trace,
                             size_t begin, size_t end) const = 0;
  bool evaluate(const std::vector<std::string> &trace) const {
    return evaluate_subt(trace, 0, trace.size());
  };
  /* Mission-time LTL (MLTL) Formula Validation Via Regular Expressions
   * https://temporallogic.org/research/WEST/WEST_extended.pdf
   * Definition 6
   */
  virtual size_t future_reach() const = 0;
  virtual size_t size() const = 0;
  virtual size_t depth() const = 0;
  virtual size_t count(ASTNode::Type target_type) const = 0;
  virtual std::unique_ptr<ASTNode> deep_copy() const = 0;
  virtual bool operator==(const ASTNode &other) const = 0;
  virtual bool operator!=(const ASTNode &other) const {
    return !(*this == other);
  }
  virtual bool operator<(const ASTNode &other) const = 0;
  bool operator>(const ASTNode &other) const { return other < *this; }
  virtual bool operator<=(const ASTNode &other) const = 0;
  bool operator>=(const ASTNode &other) const { return other <= *this; }
  virtual ~ASTNode() = default;
};

class Constant : public ASTNode {
private:
  bool val;

public:
  Constant(bool value) : val(value){};

  bool get_value() const { return val; }

  ASTNode::Type get_type() const { return ASTNode::Type::Constant; }
  std::string as_string() const { return val ? "true" : "false"; }
  bool evaluate_subt(const std::vector<std::string> &trace, size_t begin,
                     size_t end) const {
    return val;
  }
  size_t future_reach() const { return 0; }
  size_t size() const { return 1; }
  size_t depth() const { return 0; }
  size_t count(ASTNode::Type target_type) const {
    return (get_type() == target_type);
  }
  std::unique_ptr<ASTNode> deep_copy() const {
    return std::make_unique<Constant>(val);
  }
  bool operator==(const ASTNode &other) const {
    return ((get_type() == other.get_type()) &&
            (val == static_cast<const Constant &>(other).val));
  }
  bool operator<(const ASTNode &other) const {
    if (get_type() == other.get_type()) {
      return (val < static_cast<const Constant &>(other).val);
    }
    return (get_type() < other.get_type());
  }
  bool operator<=(const ASTNode &other) const {
    if (get_type() == other.get_type()) {
      return (val <= static_cast<const Constant &>(other).val);
    }
    return (get_type() <= other.get_type());
  }
};

class Variable : public ASTNode {
private:
  unsigned int id;

public:
  Variable(unsigned int id) : id(id){};

  unsigned int get_id() const { return id; }

  ASTNode::Type get_type() const { return ASTNode::Type::Variable; }
  std::string as_string() const { return 'p' + std::to_string(id); }
  bool evaluate_subt(const std::vector<std::string> &trace, size_t begin,
                     size_t end) const {
    if (begin == end || id >= trace[0].length()) {
      return false;
    }
    return (trace[begin][id] == '1');
  }
  size_t future_reach() const { return 1; }
  size_t size() const { return 1; }
  size_t depth() const { return 0; }
  size_t count(ASTNode::Type target_type) const {
    return (get_type() == target_type);
  }
  std::unique_ptr<ASTNode> deep_copy() const {
    return std::make_unique<Variable>(id);
  }
  bool operator==(const ASTNode &other) const {
    return ((get_type() == other.get_type()) &&
            (id == static_cast<const Variable &>(other).id));
  }
  bool operator<(const ASTNode &other) const {
    if (get_type() == other.get_type()) {
      return (id < static_cast<const Variable &>(other).id);
    }
    return (get_type() < other.get_type());
  }
  bool operator<=(const ASTNode &other) const {
    if (get_type() == other.get_type()) {
      return (id <= static_cast<const Variable &>(other).id);
    }
    return (get_type() <= other.get_type());
  }
};

class UnaryOp : public ASTNode {
protected:
  std::unique_ptr<ASTNode> operand;

public:
  UnaryOp() : operand(nullptr){};
  UnaryOp(std::unique_ptr<ASTNode> operand) : operand(std::move(operand)){};

  const ASTNode &get_operand() const { return *operand; }
  std::unique_ptr<ASTNode> release_operand() { return std::move(operand); }
  void set_operand(std::unique_ptr<ASTNode> new_operand) {
    operand = std::move(new_operand);
  }

  size_t size() const { return 1 + operand->size(); }
  size_t depth() const { return 1 + operand->depth(); }
  size_t count(ASTNode::Type target_type) const {
    return (get_type() == target_type) + operand->count(target_type);
  }

  virtual ASTNode::Type get_type() const = 0;
  virtual std::string as_string() const = 0;
  virtual bool evaluate_subt(const std::vector<std::string> &trace,
                             size_t begin, size_t end) const = 0;
  virtual size_t future_reach() const = 0;
  virtual std::unique_ptr<ASTNode> deep_copy() const = 0;
  virtual bool operator==(const ASTNode &other) const = 0;
  virtual bool operator<(const ASTNode &other) const = 0;
  virtual bool operator<=(const ASTNode &other) const = 0;
};

class UnaryPropOp : public UnaryOp {
public:
  using UnaryOp::UnaryOp; // inherit base-class constructors

  size_t future_reach() const { return operand->future_reach(); }
  bool operator==(const ASTNode &other) const {
    return ((get_type() == other.get_type()) &&
            (*operand == *static_cast<const UnaryPropOp &>(other).operand));
  }
  bool operator<(const ASTNode &other) const {
    if (get_type() == other.get_type()) {
      return (*operand < *static_cast<const UnaryPropOp &>(other).operand);
    }
    return (get_type() < other.get_type());
  }
  bool operator<=(const ASTNode &other) const {
    if (get_type() == other.get_type()) {
      return (*operand <= *static_cast<const UnaryPropOp &>(other).operand);
    }
    return (get_type() <= other.get_type());
  }

  virtual ASTNode::Type get_type() const = 0;
  virtual std::string as_string() const = 0;
  virtual bool evaluate_subt(const std::vector<std::string> &trace,
                             size_t begin, size_t end) const = 0;
  virtual std::unique_ptr<ASTNode> deep_copy() const = 0;
};

class Negation : public UnaryPropOp {
public:
  using UnaryPropOp::UnaryPropOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::Negation; }
  std::string as_string() const { return '~' + operand->as_string(); }
  bool evaluate_subt(const std::vector<std::string> &trace, size_t begin,
                     size_t end) const {
    return !operand->evaluate_subt(trace, begin, end);
  }
  std::unique_ptr<ASTNode> deep_copy() const {
    return std::make_unique<Negation>(operand->deep_copy());
  }
};

class UnaryTempOp : public UnaryOp {
protected:
  size_t lb, ub;

public:
  UnaryTempOp() : UnaryOp(), lb(0), ub(0){};
  UnaryTempOp(std::unique_ptr<ASTNode> operand, size_t lb, size_t ub)
      : UnaryOp(std::move(operand)), lb(lb), ub(ub){};

  size_t get_lower_bound() const { return lb; }
  size_t get_upper_bound() const { return ub; }
  void set_lower_bound(size_t new_lb) { lb = new_lb; }
  void set_upper_bound(size_t new_ub) { ub = new_ub; }

  size_t future_reach() const { return ub + operand->future_reach(); }
  bool operator==(const ASTNode &other) const {
    return ((get_type() == other.get_type()) &&
            (*operand == *static_cast<const UnaryTempOp &>(other).operand) &&
            (lb == static_cast<const UnaryTempOp &>(other).lb) &&
            (ub == static_cast<const UnaryTempOp &>(other).ub));
  }
  bool operator<(const ASTNode &other) const {
    if (get_type() == other.get_type()) {
      if (*operand == *static_cast<const UnaryTempOp &>(other).operand) {
        if (lb == static_cast<const UnaryTempOp &>(other).lb) {
          return (ub < static_cast<const UnaryTempOp &>(other).ub);
        }
        return (lb < static_cast<const UnaryTempOp &>(other).lb);
      }
      return (*operand < *static_cast<const UnaryTempOp &>(other).operand);
    }
    return (get_type() < other.get_type());
  }
  bool operator<=(const ASTNode &other) const {
    if (get_type() == other.get_type()) {
      if (*operand == *static_cast<const UnaryTempOp &>(other).operand) {
        if (lb == static_cast<const UnaryTempOp &>(other).lb) {
          return (ub <= static_cast<const UnaryTempOp &>(other).ub);
        }
        return (lb <= static_cast<const UnaryTempOp &>(other).lb);
      }
      return (*operand <= *static_cast<const UnaryTempOp &>(other).operand);
    }
    return (get_type() <= other.get_type());
  }

  virtual ASTNode::Type get_type() const = 0;
  virtual std::string as_string() const = 0;
  virtual bool evaluate_subt(const std::vector<std::string> &trace,
                             size_t begin, size_t end) const = 0;
  virtual std::unique_ptr<ASTNode> deep_copy() const = 0;
};

class Finally : public UnaryTempOp {
public:
  using UnaryTempOp::UnaryTempOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::Finally; }
  std::string as_string() const {
    return "F[" + std::to_string(lb) + ',' + std::to_string(ub) + "](" +
           operand->as_string() + ')';
  }
  bool evaluate_subt(const std::vector<std::string> &trace, size_t begin,
                     size_t end) const;
  std::unique_ptr<ASTNode> deep_copy() const {
    return std::make_unique<Finally>(operand->deep_copy(), lb, ub);
  }
};

class Globally : public UnaryTempOp {
public:
  using UnaryTempOp::UnaryTempOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::Globally; }
  std::string as_string() const {
    return "G[" + std::to_string(lb) + ',' + std::to_string(ub) + "](" +
           operand->as_string() + ')';
  }
  bool evaluate_subt(const std::vector<std::string> &trace, size_t begin,
                     size_t end) const;
  std::unique_ptr<ASTNode> deep_copy() const {
    return std::make_unique<Globally>(operand->deep_copy(), lb, ub);
  }
};

class BinaryOp : public ASTNode {
protected:
  std::unique_ptr<ASTNode> left, right;

public:
  BinaryOp() : left(nullptr), right(nullptr){};
  BinaryOp(std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right)
      : left(std::move(left)), right(std::move(right)){};

  const ASTNode &get_left() const { return *left; }
  const ASTNode &get_right() const { return *right; }
  std::unique_ptr<ASTNode> release_left() { return std::move(left); }
  std::unique_ptr<ASTNode> release_right() { return std::move(right); }
  void set_left(std::unique_ptr<ASTNode> new_left) {
    left = std::move(new_left);
  }
  void set_right(std::unique_ptr<ASTNode> new_right) {
    left = std::move(new_right);
  }

  size_t size() const { return 1 + left->size() + right->size(); }
  size_t depth() const { return 1 + std::max(left->depth(), right->depth()); }
  size_t count(ASTNode::Type target_type) const {
    return (get_type() == target_type) + left->count(target_type) +
           right->count(target_type);
  }

  virtual ASTNode::Type get_type() const = 0;
  virtual std::string as_string() const = 0;
  virtual bool evaluate_subt(const std::vector<std::string> &trace,
                             size_t begin, size_t end) const = 0;
  virtual size_t future_reach() const = 0;
  virtual std::unique_ptr<ASTNode> deep_copy() const = 0;
  virtual bool operator==(const ASTNode &other) const = 0;
  virtual bool operator<(const ASTNode &other) const = 0;
  virtual bool operator<=(const ASTNode &other) const = 0;
};

class BinaryPropOp : public BinaryOp {
public:
  using BinaryOp::BinaryOp; // inherit base-class constructor

  size_t future_reach() const {
    return std::max(left->future_reach(), right->future_reach());
  }
  bool operator==(const ASTNode &other) const {
    return ((get_type() == other.get_type()) &&
            (*left == *static_cast<const BinaryPropOp &>(other).left) &&
            (*right == *static_cast<const BinaryPropOp &>(other).right));
  }
  bool operator<(const ASTNode &other) const {
    if (get_type() == other.get_type()) {
      if (*left == *static_cast<const BinaryPropOp &>(other).left) {
        return (*right < *static_cast<const BinaryPropOp &>(other).right);
      }
      return (*left < *static_cast<const BinaryPropOp &>(other).left);
    }
    return (get_type() < other.get_type());
  }
  bool operator<=(const ASTNode &other) const {
    if (get_type() == other.get_type()) {
      if (*left == *static_cast<const BinaryPropOp &>(other).left) {
        return (*right <= *static_cast<const BinaryPropOp &>(other).right);
      }
      return (*left <= *static_cast<const BinaryPropOp &>(other).left);
    }
    return (get_type() <= other.get_type());
  }

  virtual ASTNode::Type get_type() const = 0;
  virtual std::string as_string() const = 0;
  virtual bool evaluate_subt(const std::vector<std::string> &trace,
                             size_t begin, size_t end) const = 0;
  virtual std::unique_ptr<ASTNode> deep_copy() const = 0;
};

class And : public BinaryPropOp {
public:
  using BinaryPropOp::BinaryPropOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::And; }
  std::string as_string() const {
    return '(' + left->as_string() + ")&(" + right->as_string() + ')';
  }
  bool evaluate_subt(const std::vector<std::string> &trace, size_t begin,
                     size_t end) const {
    return left->evaluate_subt(trace, begin, end) &&
           right->evaluate_subt(trace, begin, end);
  }
  std::unique_ptr<ASTNode> deep_copy() const {
    return std::make_unique<And>(left->deep_copy(), right->deep_copy());
  }
};

class Xor : public BinaryPropOp {
public:
  using BinaryPropOp::BinaryPropOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::Xor; }
  std::string as_string() const {
    return '(' + left->as_string() + ")^(" + right->as_string() + ')';
  }
  bool evaluate_subt(const std::vector<std::string> &trace, size_t begin,
                     size_t end) const {
    return left->evaluate_subt(trace, begin, end) !=
           right->evaluate_subt(trace, begin, end);
  }
  std::unique_ptr<ASTNode> deep_copy() const {
    return std::make_unique<And>(left->deep_copy(), right->deep_copy());
  }
};

class Or : public BinaryPropOp {
public:
  using BinaryPropOp::BinaryPropOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::Or; }
  std::string as_string() const {
    return '(' + left->as_string() + ")|(" + right->as_string() + ')';
  }
  bool evaluate_subt(const std::vector<std::string> &trace, size_t begin,
                     size_t end) const {
    return left->evaluate_subt(trace, begin, end) ||
           right->evaluate_subt(trace, begin, end);
  }
  std::unique_ptr<ASTNode> deep_copy() const {
    return std::make_unique<Or>(left->deep_copy(), right->deep_copy());
  }
};

class Implies : public BinaryPropOp {
public:
  using BinaryPropOp::BinaryPropOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::Implies; }
  std::string as_string() const {
    return '(' + left->as_string() + ")->(" + right->as_string() + ')';
  }
  bool evaluate_subt(const std::vector<std::string> &trace, size_t begin,
                     size_t end) const {
    return !left->evaluate_subt(trace, begin, end) ||
           right->evaluate_subt(trace, begin, end);
  }
  std::unique_ptr<ASTNode> deep_copy() const {
    return std::make_unique<Implies>(left->deep_copy(), right->deep_copy());
  }
};

class Equiv : public BinaryPropOp {
public:
  using BinaryPropOp::BinaryPropOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::Equiv; }
  std::string as_string() const {
    return '(' + left->as_string() + ")<->(" + right->as_string() + ')';
  }
  bool evaluate_subt(const std::vector<std::string> &trace, size_t begin,
                     size_t end) const {
    return left->evaluate_subt(trace, begin, end) ==
           right->evaluate_subt(trace, begin, end);
  }
  std::unique_ptr<ASTNode> deep_copy() const {
    return std::make_unique<Equiv>(left->deep_copy(), right->deep_copy());
  }
};

class BinaryTempOp : public BinaryOp {
protected:
  size_t lb, ub;

public:
  BinaryTempOp() : BinaryOp(), lb(0), ub(0){};
  BinaryTempOp(std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right,
               size_t lb, size_t ub)
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
  bool operator==(const ASTNode &other) const {
    return ((get_type() == other.get_type()) &&
            (*left == *static_cast<const BinaryTempOp &>(other).left) &&
            (*right == *static_cast<const BinaryTempOp &>(other).right) &&
            (lb == static_cast<const BinaryTempOp &>(other).lb) &&
            (ub == static_cast<const BinaryTempOp &>(other).ub));
  }
  bool operator<(const ASTNode &other) const {
    if (get_type() == other.get_type()) {
      if (*left == *static_cast<const BinaryTempOp &>(other).left) {
        if (*right == *static_cast<const BinaryTempOp &>(other).right) {
          if (lb == static_cast<const BinaryTempOp &>(other).lb) {
            return (ub < static_cast<const BinaryTempOp &>(other).ub);
          }
          return (lb < static_cast<const BinaryTempOp &>(other).lb);
        }
        return *right < *static_cast<const BinaryTempOp &>(other).right;
      }
      return (*left < *static_cast<const BinaryTempOp &>(other).left);
    }
    return (get_type() < other.get_type());
  }
  bool operator<=(const ASTNode &other) const {
    if (get_type() == other.get_type()) {
      if (*left == *static_cast<const BinaryTempOp &>(other).left) {
        if (*right == *static_cast<const BinaryTempOp &>(other).right) {
          if (lb == static_cast<const BinaryTempOp &>(other).lb) {
            return (ub <= static_cast<const BinaryTempOp &>(other).ub);
          }
          return (lb <= static_cast<const BinaryTempOp &>(other).lb);
        }
        return *right <= *static_cast<const BinaryTempOp &>(other).right;
      }
      return (*left <= *static_cast<const BinaryTempOp &>(other).left);
    }
    return (get_type() <= other.get_type());
  }

  virtual ASTNode::Type get_type() const = 0;
  virtual std::string as_string() const = 0;
  virtual bool evaluate_subt(const std::vector<std::string> &trace,
                             size_t begin, size_t end) const = 0;
  virtual std::unique_ptr<ASTNode> deep_copy() const = 0;
};

class Until : public BinaryTempOp {
public:
  using BinaryTempOp::BinaryTempOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::Until; }
  std::string as_string() const {
    return '(' + left->as_string() + ")U[" + std::to_string(lb) + ',' +
           std::to_string(ub) + "](" + right->as_string() + ')';
  }
  bool evaluate_subt(const std::vector<std::string> &trace, size_t begin,
                     size_t end) const;
  std::unique_ptr<ASTNode> deep_copy() const {
    return std::make_unique<Until>(left->deep_copy(), right->deep_copy(), lb,
                                   ub);
  }
};

class Release : public BinaryTempOp {
public:
  using BinaryTempOp::BinaryTempOp; // inherit base-class constructors

  ASTNode::Type get_type() const { return ASTNode::Type::Release; }
  std::string as_string() const {
    return '(' + left->as_string() + ")R[" + std::to_string(lb) + ',' +
           std::to_string(ub) + "](" + right->as_string() + ')';
  }
  bool evaluate_subt(const std::vector<std::string> &trace, size_t begin,
                     size_t end) const;
  std::unique_ptr<ASTNode> deep_copy() const {

    return std::make_unique<Release>(left->deep_copy(), right->deep_copy(), lb,
                                     ub);
  }
};

} // namespace libmltl
