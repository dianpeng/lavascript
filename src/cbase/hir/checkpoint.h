#ifndef CBASE_HIR_CHECKPOINT_H_
#define CBASE_HIR_CHECKPOINT_H_
#include "expr.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// Checkpoint node
//
// A checkpoint is a native machine state snapshot which later will be used to perform
// deoptimization/OSR. The following constraints are held by Checkpoint node object.
//
// 1) The checkpoint node is *generated* by a side effect node , in our case in fact
//    only extension invocation triggers the checkpoint, but we may also generate
//    checkpoint on memory access. We do speculative alias analyzing to limit the size
//    of the checkpoint and allow wider global value numbering
//
// 2) The checkpoint node captures everything on the current stack, which involves all
//    stack value , as with global variable or upvalue change it is left it untouched
//    due to the side effect intrinsic.
//
// 3) The checkpoint node is *unordered* since it already captures all the stack information
//    it needs and also gvn cannot optimize cross the bundary of checkpoint, so you shouldn't
//    see checkpoint order change. Plus even if checkpoint is unordered, we will generate
//    compensate code for checkpoint , the only issue is this will put us eagerly generate
//    too many values and spill them on stack. Or too many live variables at front for register
//    allocator.
//
// 4) The checkpoint node is overcommitted currently due to the fact we lack liveness
//    information. A better approach is to do a bytecode liveness analysis which V8 does,
//    and generate checkpoint node wrt variables that gonna be used in the future , ie
//    live.
//
//
// For GVN:
//    Checkpoint *should not* be counted as part of GVN , so if a node paritipate into
//    GVN, then its Checkpoint node should not contribute to its GVN hash or GVN equal
//    function. The node that has Checkpoint in it doesn't have any dependency with this
//    Checkpoint , it just means when node bailout, it uses this Checkpoint to reconstruct
//    interpreter states/frame. But this is not dependent.
//
LAVA_CBASE_HIR_DEFINE(Checkpoint,public Expr) {
 public:
  inline static Checkpoint* New( Graph* , IRInfo* );
  // add a stack traced value into checkpoint object
  inline void AddStackSlot( Expr* , std::uint32_t );
  // return the ir_info object
  IRInfo* ir_info() const { return ir_info_; }

  Checkpoint( Graph* graph , std::uint32_t id , IRInfo* info ):
    Expr    (HIR_CHECKPOINT,id,graph),
    ir_info_(info)
  {}
 private:
  IRInfo* ir_info_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Checkpoint)
};

LAVA_CBASE_HIR_DEFINE(StackSlot,public Expr) {
 public:
  inline static StackSlot* New( Graph* , Expr* , std::uint32_t );
  std::uint32_t index() const { return index_; }
  Expr*          expr() const { return operand_list()->First(); }
  StackSlot( Graph* graph , std::uint32_t id , Expr* expr , std::uint32_t index ):
    Expr(HIR_STACK_SLOT,id,graph),
    index_(index)
  {
    AddOperand(expr);
  }
 private:
  std::uint32_t index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(StackSlot)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_CHECKPOINT_H_
