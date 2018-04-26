#ifndef CBASE_HIR_ARITH_H_
#define CBASE_HIR_ARITH_H_
#include "expr.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// ----------------------------------------------------------------
// Arithmetic
//
// The following node doesn't map the operands to a specific type so
// they map to dynamic dispatch code in jitted version. For binary,
// since we support invoke binary operator implemented in C++ side,
// the binary node is even a side effect node and it will generate
// a checkpoint/framestate.
// ----------------------------------------------------------------
class Unary : public Expr {
 public:
  enum Operator { MINUS, NOT };
  inline static Unary* New( Graph* , Expr* , Operator );
  inline static Operator BytecodeToOperator( interpreter::Bytecode bc );
  inline static const char* GetOperatorName( Operator op );
 public:
  Expr*       operand() const { return operand_list()->First(); }
  Operator       op  () const { return op_;      }
  const char* op_name() const { return GetOperatorName(op()); }
  Unary( Graph* graph , std::uint32_t id , Expr* opr , Operator op ):
    Expr  (HIR_UNARY,id,graph),
    op_   (op)
  {
    AddOperand(opr);
  }
 protected:
  Unary( IRType type , Graph* graph , std::uint32_t id , Expr* opr , Operator op ):
    Expr  (type,id,graph),
    op_   (op)
  {
    AddOperand(opr);
  }
 private:
  Operator   op_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Unary)
};

class Binary : public Expr {
 public:
  enum Operator {
    // arithmetic
    ADD, SUB, MUL, DIV, MOD, POW,
    // comparison
    LT , LE , GT , GE , EQ , NE ,
    // logic
    AND, OR ,
    // bitwise operators
    BAND, BOR , BXOR, BSHL, BSHR, BROL, BROR
  };

  inline static bool        IsComparisonOperator( Operator );
  inline static bool        IsArithmeticOperator( Operator );
  inline static bool        IsBitwiseOperator   ( Operator );
  inline static bool        IsLogicOperator     ( Operator );
  inline static Operator    BytecodeToOperator  ( interpreter::Bytecode );
  inline static const char* GetOperatorName     ( Operator );
 public:
  // Create a binary node
  inline static Binary* New( Graph* , Expr* , Expr* , Operator );
  Expr*           lhs() const { return operand_list()->First(); }
  Expr*           rhs() const { return operand_list()->Last (); }
  Operator         op() const { return op_;  }
  const char* op_name() const { return GetOperatorName(op()); }
  Binary( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , Operator op ):
    Expr  (HIR_BINARY,id,graph),
    op_   (op)
  {
    AddOperand(lhs);
    AddOperand(rhs);
  }
 protected:
  Binary( IRType irtype ,Graph* graph ,std::uint32_t id ,Expr* lhs ,Expr* rhs ,Operator op ):
    Expr  (irtype,id,graph),
    op_   (op)
  {
    AddOperand(lhs);
    AddOperand(rhs);
  }
 private:
  Operator op_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Binary)
};

class Ternary: public Expr {
 public:
  inline static Ternary* New( Graph* , Expr* , Expr* , Expr* );
  Ternary( Graph* graph , std::uint32_t id , Expr* cond , Expr* lhs , Expr* rhs ):
    Expr  (HIR_TERNARY,id,graph)
  {
    AddOperand(cond);
    AddOperand(lhs);
    AddOperand(rhs);
  }

  Expr* condition() const { return operand_list()->First(); }
  Expr* lhs      () const { return operand_list()->Index(1); }
  Expr* rhs      () const { return operand_list()->Last(); }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Ternary)
};

/* -------------------------------------------------------
 * Low level operations
 * ------------------------------------------------------*/
class Float64Negate  : public Expr {
 public:
  inline static Float64Negate* New( Graph* , Expr* );
  Expr* operand() const { return operand_list()->First(); }
  Float64Negate( Graph* graph , std::uint32_t id , Expr* opr ):
    Expr(HIR_FLOAT64_NEGATE,id,graph)
  {
    AddOperand(opr);
  }
 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),operand()->GVNHash());
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->IsFloat64Negate()) {
      auto that_negate = that->AsFloat64Negate();
      return operand()->Equal(that_negate->operand());
    }
    return false;
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64Negate)
};

// Specialized logic operator, its lhs is type fixed by boolean. ie, we are
// sure its lhs operand outputs boolean in an unboxed format.
class BooleanNot: public Expr {
 public:
  inline static BooleanNot* New( Graph* , Expr* );
  Expr* operand() const { return operand_list()->First(); }
  BooleanNot( Graph* graph , std::uint32_t id , Expr* opr ):
    Expr(HIR_BOOLEAN_NOT,id,graph)
  {
    AddOperand(opr);
  }

 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),operand()->GVNHash());
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->IsBooleanNot()) {
      auto that_negate = that->AsBooleanNot();
      return operand()->Equal(that_negate->operand());
    }
    return false;
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(BooleanNot)
};

namespace detail {

template< typename T > struct Float64BinaryGVNImpl {
 protected:
  std::uint64_t GVNHashImpl() const;
  bool EqualImpl( const Expr* that ) const;
};

} // namespace detail

class Float64Arithmetic : public Binary , public detail::Float64BinaryGVNImpl<Float64Arithmetic> {
 public:
  using Binary::Operator;
  inline static Float64Arithmetic* New( Graph* , Expr*, Expr*, Operator );
  Float64Arithmetic( Graph* graph , std::uint32_t id , Expr* lhs, Expr* rhs, Operator op ):
    Binary(HIR_FLOAT64_ARITHMETIC,graph,id,lhs,rhs,op)
  {
    lava_debug(NORMAL,lava_verify(Binary::IsArithmeticOperator(op)););
  }

 public:
  virtual std::uint64_t GVNHash()        const { return GVNHashImpl(); }
  virtual bool Equal( const Expr* that ) const { return EqualImpl(that); }

 private:

  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64Arithmetic)
};

class Float64Bitwise: public Binary , public detail::Float64BinaryGVNImpl<Float64Bitwise> {
 public:
  using Binary::Operator;
  inline static Float64Bitwise* New( Graph* , Expr*, Expr*, Operator );
  Float64Bitwise( Graph* graph , std::uint32_t id , Expr* lhs, Expr* rhs, Operator op ):
    Binary(HIR_FLOAT64_BITWISE,graph,id,lhs,rhs,op)
  {
    lava_debug(NORMAL,lava_verify(Binary::IsBitwiseOperator(op)););
  }

 public:
  virtual std::uint64_t GVNHash()        const { return GVNHashImpl(); }
  virtual bool Equal( const Expr* that ) const { return EqualImpl(that); }

 private:

  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64Bitwise)
};

class Float64Compare : public Binary , public detail::Float64BinaryGVNImpl<Float64Compare> {
 public:
  using Binary::Operator;
  inline static Float64Compare* New( Graph* , Expr* , Expr* , Operator );
  Float64Compare( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , Operator op ):
    Binary(HIR_FLOAT64_COMPARE,graph,id,lhs,rhs,op)
  {
    lava_debug(NORMAL,lava_verify(Binary::IsComparisonOperator(op)););
  }

 public:
  virtual std::uint64_t GVNHash()        const { return GVNHashImpl(); }
  virtual bool Equal( const Expr* that ) const { return EqualImpl(that); }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64Compare)
};

class StringCompare : public Binary , public detail::Float64BinaryGVNImpl<StringCompare> {
 public:
  using Binary::Operator;
  inline static StringCompare* New( Graph* , Expr* , Expr* , Operator );
  StringCompare( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , Operator op ):
    Binary(HIR_STRING_COMPARE,graph,id,lhs,rhs,op)
  {
    lava_debug(NORMAL,lava_verify(Binary::IsComparisonOperator(op)););
  }
 public:
  virtual std::uint64_t GVNHash()        const { return GVNHashImpl(); }
  virtual bool Equal( const Expr* that ) const { return EqualImpl(that); }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(StringCompare);
};

class SStringEq : public Binary , public detail::Float64BinaryGVNImpl<SStringEq> {
 public:
  inline static SStringEq* New( Graph* , Expr* , Expr* );
  SStringEq( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs ):
    Binary(HIR_SSTRING_EQ,graph,id,lhs,rhs,Binary::EQ)
  {}

 public:
  virtual std::uint64_t GVNHash()        const { return GVNHashImpl(); }
  virtual bool Equal( const Expr* that ) const { return EqualImpl(that); }
};

class SStringNe : public Binary , public detail::Float64BinaryGVNImpl<SStringNe> {
 public:
  inline static SStringNe* New( Graph* , Expr* , Expr* );
  SStringNe( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs ):
    Binary(HIR_SSTRING_EQ,graph,id,lhs,rhs,Binary::NE)
  {}

 public:
  virtual std::uint64_t GVNHash()        const { return GVNHashImpl(); }
  virtual bool Equal( const Expr* that ) const { return EqualImpl(that); }
};

class BooleanLogic : public Binary {
 public:
   inline static BooleanLogic* New( Graph* , Expr* , Expr* , Operator op );
   BooleanLogic( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , Operator op ):
     Binary(HIR_BOOLEAN_LOGIC,graph,id,lhs,rhs,op)
  {
    lava_debug(NORMAL,lava_verify( GetTypeInference(lhs) == TPKIND_BOOLEAN &&
                                   GetTypeInference(rhs) == TPKIND_BOOLEAN ););
  }

 public:
  virtual std::uint64_t GVNHash()        const {
    return GVNHash3(type_name(), lhs()->GVNHash(), op (), rhs()->GVNHash());
  }
  virtual bool Equal( const Expr* that ) const {
    if(that->IsBooleanLogic()) {
      auto bl = that->AsBooleanLogic();
      return bl->op() == op() && bl->lhs()->Equal(lhs()) && bl->rhs()->Equal(rhs());
    }
    return false;
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(BooleanLogic)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript
#endif // CBASE_HIR_ARITH_H_
