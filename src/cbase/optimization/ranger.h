#ifndef CBASE_OPTIMIZATION_RANGER_H_
#define CBASE_OPTIMIZATION_RANGER_H_
#include "src/cbase/hir-pass.h"

namespace lavascript {
namespace cbase      {
namespace hir        {


/**
 * Ranger pass mark all the branch condition node with value range object
 * later on can be used to do inference
 */

class Ranger : public HIRPass {
 public:
  virtual bool Perform( Graph* , HIRPass::Flag );

  Ranger() : HIRPass( "ranger" ) {}
};


} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_OPTIMIZATION_RANGER_H_
