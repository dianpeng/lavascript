#ifndef CBASE_PASS_LOOP_INDUCTION_H_
#define CBASE_PASS_LOOP_INDUCTION_H_
#include "src/cbase/hir-pass.h"

namespace lavascript {
namespace cbase      {
namespace hir        {


// This pass does typping the loop induction variable with selective type coearsion
class LoopInduction : public HIRPass {
 public:
  virtual bool Perform( hir::Graph* , Flag );
  LoopInduction() : HIRPass("loop-induction") {}
};

} // namespace hir
} // namespace cbase
} // namespace lavascript


#endif // CBASE_PASS_LOOP_INDUCTION_H_
