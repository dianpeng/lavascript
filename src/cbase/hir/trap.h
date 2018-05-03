#ifndef CBASE_HIR_TRAP_H_
#define CBASE_HIR_TRAP_H_
#include "control-flow.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// Represent fallback to interpreter, manually generate this node means we want to
// abort at current stage in the graph.
class Trap : public ControlFlow {
 public:
  inline static Trap* New( Graph* , Expr*  , Checkpoint* , ControlFlow* );
  // create a Trap node that always trap or its condition is false literal
  inline static Trap* New( Graph* , Checkpoint* , ControlFlow* );

  Expr*       condition () const { return operand_list()->First(); }
  Checkpoint* checkpoint() const { return operand_list()->Last ()->AsCheckpoint(); }

  Trap( Graph* graph , std::uint32_t id , Expr* condition , Checkpoint* cp , ControlFlow* region ):
    ControlFlow(HIR_TRAP,id,graph,region)
  {
    AddOperand(condition);
    AddOperand(cp);
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Trap)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_TRAP_H_
