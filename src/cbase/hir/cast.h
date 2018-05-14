#ifndef CBASE_HIR_CAST_H_
#define CBASE_HIR_CAST_H_
#include "expr.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

/*
 *
 * There're 2 types of operations :
 * 1) ConvXXX , basically this is a high level operation which mapps a *boxed* value
 *    into XXX type in a boxed format as well. It requires a dynamic dispatch code
 *    stub which is relative costy.
 *
 *    This operation takes a *boxed* value and do the cast and then produce a unboxed
 *    value.
 *
 * 2) ToXXX  , this is a low level operation which convers things like cast a double
 *    into a integer number, on x64 this mapps to assembly like cvtsd2si/cvtsi2sd in
 *    SSE. The input and output to this operation are always unboxed value.
 */

LAVA_CBASE_HIR_DEFINE(ConvBoolean,public Expr) {
 public:
  // New a normal conv boolean operation, takes a boxed input and produce a unboxed
  // output.
  inline static ConvBoolean* New ( Graph* , Expr* );

  // New a conv boolean operation which takes a boxed input and produce a boxed output
  // as well.
  inline static Box*         NewBox( Graph* , Expr* );

  Expr* value() const { return operand_list()->First(); }

  ConvBoolean( Graph* graph , std::uint32_t id , Expr* value ):
    Expr(HIR_CONV_BOOLEAN,id,graph)
  { AddOperand(value); }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ConvBoolean)
};

// convert a boxed expression into a negative boolean. Basically if the input evaluates
// to be true , then this node return false in unbox version ; otherwise it returns true.
LAVA_CBASE_HIR_DEFINE(ConvNBoolean,public Expr) {
 public:
  inline static ConvNBoolean* New   ( Graph* , Expr* );
  inline static Box*          NewBox( Graph* , Expr* );

  Expr* value() const { return operand_list()->First(); }

  ConvNBoolean( Graph* graph , std::uint32_t id , Expr* value ):
    Expr(HIR_CONV_NBOOLEAN,id,graph)
  { AddOperand(value); }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ConvNBoolean)
};


} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_CAST_H_
