#ifndef CBASE_HIR_ARITH_H_
#define CBASE_HIR_ARITH_H_
#include "effect.h"
#include "src/all-static.h"

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

class Binary : public AllStatic {
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
  inline static bool        IsLogicalOperator   ( Operator );
  inline static Operator    BytecodeToOperator  ( interpreter::Bytecode );
  inline static const char* GetOperatorName     ( Operator );
};

// A virtual base class to let all different types of Binary node have similar
// APIs/interfaces
class BinaryNode {
 public:
  virtual Expr*             lhs() const = 0;
  virtual Expr*             rhs() const = 0;
  virtual Binary::Operator  op () const = 0;
  const char*          op_name () const { return Binary::GetOperatorName(op()); }
  virtual ~BinaryNode () {}
};

// DynamicBinary represents a dynamic dispatched binary operation node. This node generates
// a effect barrier and also generates a checkpoint because of the side effect
class DynamicBinary : public WriteBarrier , public BinaryNode {
 public:
  typedef Binary::Operator Operator;

  DynamicBinary( IRType type , Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs ,
                                                                             Binary::Operator op ):
    WriteBarrier(type ,graph ,id),
    op_         (op)
  {
    AddOperand(lhs);
    AddOperand(rhs);
  }

  virtual Expr*            lhs() const { return operand_list()->First(); }
  virtual Expr*            rhs() const { return operand_list()->Last (); }
  virtual Binary::Operator op () const { return op_; }
 private:
  Binary::Operator op_;
};

class Arithmetic : public DynamicBinary {
 public:
  static inline Arithmetic* New( Graph* , Expr* , Expr* , Binary::Operator );

  Arithmetic( Graph* graph , std::uint32_t id , Expr* lhs, Expr* rhs, Binary::Operator op ) :
    DynamicBinary( HIR_ARITHMETIC , graph, id, lhs, rhs, op )
  {
    lava_debug(NORMAL,lava_verify(Binary::IsArithmeticOperator(op)););
  }
};

class Compare: public DynamicBinary {
 public:
  static inline Compare* New( Graph* , Expr* , Expr* , Binary::Operator );

  Compare( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , Binary::Operator op ):
    DynamicBinary( HIR_COMPARE, graph, id, lhs, rhs, op )
  {
    lava_debug(NORMAL,lava_verify(Binary::IsComparisonOperator(op)););
  }
};

// This is a binary node but it is not a dynamic dispatched node so not inherit from the
// the dynamic binary node. logical node is a normal node which will not do dynamic dispatch
class Logical : public Expr , public BinaryNode {
 public:
  typedef Binary::Operator Operator;

  static inline Logical* New( Graph* , Expr* , Expr* , Binary::Operator );

  Logical( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , Binary::Operator op ):
   Expr(HIR_LOGICAL,id,graph),
   op_ (op)
  {
    lava_debug(NORMAL,lava_verify(Binary::IsLogicalOperator(op)););
    AddOperand(lhs);
    AddOperand(rhs);
  }

  virtual Expr*            lhs() const { return operand_list()->First(); }
  virtual Expr*            rhs() const { return operand_list()->Last (); }
  virtual Binary::Operator op () const { return op_; }
 private:
  Binary::Operator op_;
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

// SpecializeBinary represents all binary operation that is specialized
// with type information builtin. These nodes take into unboxed value and
// generate unboxed value
class SpecializeBinary : public Expr , public BinaryNode {
 public:
  typedef Binary::Operator Operator;

  SpecializeBinary( IRType type , Graph* graph , std::uint32_t id , Expr* lhs ,
                                                                    Expr* rhs ,
                                                                    Binary::Operator op ):
    Expr(type,id,graph),
    op_ (op)
  {
    AddOperand(lhs);
    AddOperand(rhs);
  }

  virtual Expr*            lhs() const { return operand_list()->First(); }
  virtual Expr*            rhs() const { return operand_list()->Last (); }
  virtual Binary::Operator op () const { return op_; }

  // GVN implementation
  virtual std::uint64_t GVNHash ()              const;
  virtual bool          Equal   ( const Expr* ) const;

 private:
  Binary::Operator op_;
};

class Float64Arithmetic : public SpecializeBinary {
 public:
  typedef Binary::Operator Operator;

  inline static Float64Arithmetic* New( Graph* , Expr*, Expr*, Operator );

  Float64Arithmetic( Graph* graph , std::uint32_t id , Expr* lhs, Expr* rhs, Operator op ):
    SpecializeBinary(HIR_FLOAT64_ARITHMETIC,graph,id,lhs,rhs,op)
  {
    lava_debug(NORMAL,lava_verify(Binary::IsArithmeticOperator(op)););
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64Arithmetic)
};

class Float64Bitwise: public SpecializeBinary {
 public:
  typedef Binary::Operator Operator;

  inline static Float64Bitwise* New( Graph* , Expr*, Expr*, Operator );

  Float64Bitwise( Graph* graph , std::uint32_t id , Expr* lhs, Expr* rhs, Operator op ):
    SpecializeBinary(HIR_FLOAT64_BITWISE,graph,id,lhs,rhs,op)
  {
    lava_debug(NORMAL,lava_verify(Binary::IsBitwiseOperator(op)););
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64Bitwise)
};

class Float64Compare : public SpecializeBinary {
 public:
  typedef Binary::Operator Operator;

  inline static Float64Compare* New( Graph* , Expr* , Expr* , Operator );

  Float64Compare( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , Operator op ):
    SpecializeBinary(HIR_FLOAT64_COMPARE,graph,id,lhs,rhs,op)
  {
    lava_debug(NORMAL,lava_verify(Binary::IsComparisonOperator(op)););
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64Compare)
};

class StringCompare : public SpecializeBinary {
 public:
  typedef Binary::Operator Operator;
  inline static StringCompare* New( Graph* , Expr* , Expr* , Operator );
  StringCompare( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , Operator op ):
    SpecializeBinary(HIR_STRING_COMPARE,graph,id,lhs,rhs,op)
  {
    lava_debug(NORMAL,lava_verify(Binary::IsComparisonOperator(op)););
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(StringCompare);
};

class SStringEq : public SpecializeBinary {
 public:
  inline static SStringEq* New( Graph* , Expr* , Expr* );
  SStringEq( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs ):
    SpecializeBinary(HIR_SSTRING_EQ,graph,id,lhs,rhs,Binary::EQ)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(SStringEq)
};

class SStringNe : public SpecializeBinary {
 public:
  inline static SStringNe* New( Graph* , Expr* , Expr* );
  SStringNe( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs ):
    SpecializeBinary(HIR_SSTRING_EQ,graph,id,lhs,rhs,Binary::NE)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(SStringNe)
};

class BooleanLogic : public SpecializeBinary {
 public:
   inline static BooleanLogic* New( Graph* , Expr* , Expr* , Operator op );
   BooleanLogic( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , Operator op ):
     SpecializeBinary(HIR_BOOLEAN_LOGIC,graph,id,lhs,rhs,op)
  {
    lava_debug(NORMAL,lava_verify( GetTypeInference(lhs) == TPKIND_BOOLEAN &&
                                   GetTypeInference(rhs) == TPKIND_BOOLEAN ););
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(BooleanLogic)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript
#endif // CBASE_HIR_ARITH_H_
