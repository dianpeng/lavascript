#ifndef CBASE_OPTIMIZATION_GUARD_ELIMINATE_H_
#define CBASE_OPTIMIZATION_GUARD_ELIMINATE_H_

#include "src/cbase/hir-pass.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

class GuardEliminate : public HIRPass {
 public:
  virtual bool Perform( Graph* , HIRPass::Flag );
  GuardEliminate() : HIRPass("guard-eliminate") {}
};


} // namespace hir
} // namespace cbase
} // namespace hir

#endif // CBASE_OPTIMIZATION_GUARD_ELIMINATE_H_
