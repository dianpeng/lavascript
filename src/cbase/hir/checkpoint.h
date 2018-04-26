#ifndef CBASE_HIR_CHECKPOINT_H_
#define CBASE_HIR_CHECKPOINT_H_
#include "expr.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// Checkpoint node
//
// A checkpoint node will capture the VM/Interpreter state. When an irnode
// needs a bailout for speculative execution , it needs Checkpoint node.
//
// The checkpoint node will impact CFG generation/scheduling , anynode that
// is referenced by checkpoint must be scheduled before the node that actually
// reference this checkpoint node.
//
// For VM/Interpreter state , there're only 3 types of states
//
// 1) a register stack state , represented by StackSlot node
// 2) a upvalue state , represented by UGetSlot node
// 3) a global value state , currently we don't have any optimization against
//    global values, so they are not needed to be captured in the checkpoint
//    they are more like volatile in C/C++ , always read from its memory and
//    write through back to where it is
//
class Checkpoint : public Expr {
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

// StackSlot node
//
// Represent a value must be flushed back a certain stack slot when will
// bailout from the interpreter
//
// It is only used inside of the checkpoint nodes
class StackSlot : public Expr {
 public:
  inline static StackSlot* New( Graph* , Expr* , std::uint32_t );
  std::uint32_t index() const { return index_; }
  Expr* expr() const { return operand_list()->First(); }

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
