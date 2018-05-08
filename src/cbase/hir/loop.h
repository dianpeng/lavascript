#ifndef CBASE_HIR_LOOP_H_
#define CBASE_HIR_LOOP_H_
#include "expr.h"
#include "control-flow.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

#if 0

// Special node to represent loop variable ( induction variable ). The main
// reason for this is to simplify the type inference code.
//
// A loop induction variable will generate a Phi node with self reference,
// eg :
//
// [ Phi ] --> [ Add ] --> [ Step ]
//   /\          ||
//   ||          ||
//   ||          \/
//   --------------
//      backedge
//
//
// This makes simple recursive type inference extreamly hard to achieve,
// obviously this will make cycle if we do recursive without marking, we
// can do a mark algorithm but this will be expensive. The type inference
// in our code should be as simple as possible since it is on the fly and
// may not yield any benefits. The currently agorithm doesn't do any types
// of recursive call , for phi it bailout when find a cycle. To make induction
// variable typped, we use this LoopVar node , this also saves us from
// finding induction variable in later loop phase.
class LoopVar : public Expr {
 public:
  inline static LoopVar* New( Graph* );

  LoopVar( Graph* graph , std::uint32_t id ):
    Expr      (HIR_LOOP_VAR,id,graph),
    type_kind_(TPKIND_UNKNOWN)
  {}

  // type kind for this loop variable, unknown means top
  TypeKind type_kind() const { return type_kind_; }

 private:
  TypeKind type_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopVar)
};

#endif

// --------------------------------------------------------------------------
//
// Loop related blocks
//
// --------------------------------------------------------------------------
class LoopHeader : public ControlFlow {
 public:
  inline static LoopHeader* New( Graph* , ControlFlow* );

  Expr* condition() const { return operand_list()->First(); }
  void set_condition( Expr* condition ) {
    lava_debug(NORMAL,lava_verify(operand_list()->empty()););
    AddOperand(condition);
  }

  ControlFlow* merge() const { return merge_; }
  void set_merge( ControlFlow* merge ) { merge_ = merge; }

  LoopHeader( Graph* graph , std::uint32_t id , ControlFlow* region ):
    ControlFlow(HIR_LOOP_HEADER,id,graph,region)
  {}
 private:
  ControlFlow* merge_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopHeader);
};

class Loop : public ControlFlow {
 public:
  inline static Loop* New( Graph* );

  Loop( Graph* graph , std::uint32_t id ):
    ControlFlow(HIR_LOOP,id,graph)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Loop)
};

class LoopExit : public ControlFlow {
 public:
  inline static LoopExit* New( Graph* , Expr* );
  Expr* condition() const { return operand_list()->First(); }

  LoopExit( Graph* graph , std::uint32_t id , Expr* cond ):
    ControlFlow(HIR_LOOP_EXIT,id,graph)
  {
    AddOperand(cond);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopExit)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript
#endif // CBASE_HIR_LOOP_H_
