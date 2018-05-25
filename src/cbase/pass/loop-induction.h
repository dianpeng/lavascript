#ifndef CBASE_PASS_LOOP_INDUCTION_H_
#define CBASE_PASS_LOOP_INDUCTION_H_
#include "src/cbase/hir-pass.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// LoopInduction pass for loop induction variable. During graph construction
// phase all the loop induction variable are marked with special node
// called LoopIV. This phase will implement a backwards propogation
// algorithm to specialize LoopIV with certain type when it is applicable.

class LoopInduction : public HIRPass {
 public:
  virtual bool Perform( hir::Graph* , Flag );
  LoopInduction() : HIRPass("loop-induction") {}
};

} // namespace hir
} // namespace cbase
} // namespace lavascript


#endif // CBASE_PASS_LOOP_INDUCTION_H_
