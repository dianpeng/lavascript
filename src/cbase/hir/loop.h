#ifndef CBASE_HIR_LOOP_H_
#define CBASE_HIR_LOOP_H_
#include "control-flow.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

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