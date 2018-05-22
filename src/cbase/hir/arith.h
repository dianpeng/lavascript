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
LAVA_CBASE_HIR_DEFINE(Tag=UNARY;Name="unary";Leaf=NoLeaf;Effect=NoEffect,
    Unary,public Expr) {
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
LAVA_CBASE_HIR_DEFINE(NO_META,DynamicBinary,public HardBarrier,public BinaryNode) {
 public:
  using Operator = Binary::Operator;

  DynamicBinary( IRType type , std::uint32_t id , Graph* graph , Expr* lhs , Expr* rhs ,
                                                                             Binary::Operator op ):
    HardBarrier(type ,id, graph),
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

LAVA_CBASE_HIR_DEFINE(Tag=ARITHMETIC;Name="arithmetic";Leaf=NoLeaf;Effect=Effect,
    Arithmetic,public DynamicBinary) {
 public:
  static inline Arithmetic* New( Graph* , Expr* , Expr* , Binary::Operator );

  Arithmetic( Graph* graph , std::uint32_t id , Expr* lhs, Expr* rhs, Binary::Operator op ) :
    DynamicBinary( HIR_ARITHMETIC , id, graph, lhs, rhs, op )
  {
    lava_debug(NORMAL,lava_verify(Binary::IsArithmeticOperator(op)););
  }
};

LAVA_CBASE_HIR_DEFINE(Tag=COMPARE;Name="compare";Leaf=NoLeaf;Effect=Effect,
    Compare,public DynamicBinary) {
 public:
  static inline Compare* New( Graph* , Expr* , Expr* , Binary::Operator );

  Compare( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , Binary::Operator op ):
    DynamicBinary( HIR_COMPARE, id, graph, lhs, rhs, op )
  {
    lava_debug(NORMAL,lava_verify(Binary::IsComparisonOperator(op)););
  }
};

// This is a binary node but it is not a dynamic dispatched node so not inherit from the
// the dynamic binary node. logical node is a normal node which will not do dynamic dispatch
LAVA_CBASE_HIR_DEFINE(Tag=LOGICAL;Name="logical";Leaf=NoLeaf;Effect=NoEffect,
    Logical,public Expr,public BinaryNode) {
 public:
  using Operator = Binary::Operator;

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

LAVA_CBASE_HIR_DEFINE(Tag=TERNARY;Name="ternary";Leaf=NoLeaf;Effect=NoEffect,
    Ternary,public Expr) {
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
  Expr* lhs      () const { return Operand(1); }
  Expr* rhs      () const { return operand_list()->Last(); }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Ternary)
};

/* -------------------------------------------------------
 * Low level operations
 * ------------------------------------------------------*/
LAVA_CBASE_HIR_DEFINE(Tag=FLOAT64_NEGATE;Name="float64_negate";Leaf=NoLeaf;Effect=NoEffect,
    Float64Negate,public Expr) {
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
LAVA_CBASE_HIR_DEFINE(Tag=BOOLEAN_NOT;Name="boolean_not";Leaf=NoLeaf;Effect=NoEffect,
    BooleanNot,public Expr) {
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
LAVA_CBASE_HIR_DEFINE(NO_META,SpecializeBinary,public Expr,public BinaryNode) {
 public:
  using Operator = Binary::Operator;
  SpecializeBinary( IRType type , std::uint32_t id , Graph* graph , Expr* lhs ,
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

LAVA_CBASE_HIR_DEFINE(Tag=FLOAT64_ARITHMETIC;Name="float64_arithmetic";Leaf=NoLeaf;Effect=NoEffect,
    Float64Arithmetic,public SpecializeBinary) {
 public:
  using Operator = Binary::Operator;

  inline static Float64Arithmetic* New( Graph* , Expr*, Expr*, Operator );

  Float64Arithmetic( Graph* graph , std::uint32_t id , Expr* lhs, Expr* rhs, Operator op ):
    SpecializeBinary(HIR_FLOAT64_ARITHMETIC,id,graph,lhs,rhs,op)
  {
    lava_debug(NORMAL,lava_verify(Binary::IsArithmeticOperator(op)););
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64Arithmetic)
};

LAVA_CBASE_HIR_DEFINE(Tag=FLOAT64_BITWISE;Name="float64_bitwise";Leaf=NoLeaf;Effect=NoEffect,
    Float64Bitwise,public SpecializeBinary) {
 public:
  using Operator = Binary::Operator;

  inline static Float64Bitwise* New( Graph* , Expr*, Expr*, Operator );

  Float64Bitwise( Graph* graph , std::uint32_t id , Expr* lhs, Expr* rhs, Operator op ):
    SpecializeBinary(HIR_FLOAT64_BITWISE,id,graph,lhs,rhs,op)
  {
    lava_debug(NORMAL,lava_verify(Binary::IsBitwiseOperator(op)););
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64Bitwise)
};

LAVA_CBASE_HIR_DEFINE(Tag=FLOAT64_COMPARE;Name="float64_compare";Leaf=NoLeaf;Effect=NoEffect,
    Float64Compare,public SpecializeBinary) {
 public:
  using Operator = Binary::Operator;

  inline static Float64Compare* New( Graph* , Expr* , Expr* , Operator );

  Float64Compare( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , Operator op ):
    SpecializeBinary(HIR_FLOAT64_COMPARE,id,graph,lhs,rhs,op)
  {
    lava_debug(NORMAL,lava_verify(Binary::IsComparisonOperator(op)););
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64Compare)
};

LAVA_CBASE_HIR_DEFINE(Tag=STRING_COMPARE;Name="string_compare";Leaf=NoLeaf;Effect=NoEffect,
    StringCompare,public SpecializeBinary) {
 public:
  using Operator = Binary::Operator;
  inline static StringCompare* New( Graph* , Expr* , Expr* , Operator );
  StringCompare( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , Operator op ):
    SpecializeBinary(HIR_STRING_COMPARE,id,graph,lhs,rhs,op)
  {
    lava_debug(NORMAL,lava_verify(Binary::IsComparisonOperator(op)););
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(StringCompare);
};

LAVA_CBASE_HIR_DEFINE(Tag=SSTRING_EQ;Name="sstring_eq";Leaf=NoLeaf;Effect=NoEffect,
    SStringEq,public SpecializeBinary) {
 public:
  inline static SStringEq* New( Graph* , Expr* , Expr* );
  SStringEq( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs ):
    SpecializeBinary(HIR_SSTRING_EQ,id,graph,lhs,rhs,Binary::EQ)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(SStringEq)
};

LAVA_CBASE_HIR_DEFINE(Tag=SSTRING_NE;Name="sstring_ne";Leaf=NoLeaf;Effect=NoEffect,
    SStringNe,public SpecializeBinary) {
 public:
  inline static SStringNe* New( Graph* , Expr* , Expr* );
  SStringNe( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs ):
    SpecializeBinary(HIR_SSTRING_EQ,id,graph,lhs,rhs,Binary::NE)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(SStringNe)
};

LAVA_CBASE_HIR_DEFINE(Tag=BOOLEAN_LOGIC;Name="boolean_logic";Leaf=NoLeaf;Effect=NoEffect,
    BooleanLogic,public SpecializeBinary) {
 public:
   inline static BooleanLogic* New( Graph* , Expr* , Expr* , Operator op );
   BooleanLogic( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , Operator op ):
     SpecializeBinary(HIR_BOOLEAN_LOGIC,id,graph,lhs,rhs,op)
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
