#ifndef CBASE_HIR_TRAP_H_
#define CBASE_HIR_TRAP_H_
#include "control-flow.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// Represent fallback to interpreter, manually generate this node means we want to
// abort at current stage in the graph.
LAVA_CBASE_HIR_DEFINE(Trap,public ControlFlow) {
 public:
  inline static Trap* New( Graph* , Checkpoint* , ControlFlow* );

  Checkpoint* checkpoint() const { return operand_list()->First()->AsCheckpoint(); }

  Trap( Graph* graph , std::uint32_t id , Checkpoint* cp , ControlFlow* region ):
    ControlFlow(HIR_TRAP,id,graph,region)
  {
    AddOperand(cp);
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Trap)
};


LAVA_CBASE_HIR_DEFINE(CondTrap,public ControlFlow) {
 public:
  inline static CondTrap* New( Graph* , Test* , Checkpoint* , ControlFlow* );

  Test*             test() const { return operand_list()->First()->AsTest();      }
  Checkpoint* checkpoint() const { return operand_list()->Last()->AsCheckpoint(); }

  CondTrap( Graph* graph , std::uint32_t id , Test* test , Checkpoint* cp , ControlFlow* region ):
    ControlFlow(HIR_COND_TRAP,id,graph,region)
  {
    AddOperand(test);
    AddOperand(cp);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(CondTrap)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_TRAP_H_
