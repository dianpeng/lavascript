#ifndef CBASE_OPTIMIZATION_INFER_H_
#define CBASE_OPTIMIZATION_INFER_H_
#include "src/cbase/hir-pass.h"

namespace lavascript {
namespace cbase      {
namespace hir        {


/**
 * Infer pass mark all the branch condition node with value range object
 * later on can be used to do inference
 */

class Infer : public HIRPass {
 public:
  virtual bool Perform( Graph* , HIRPass::Flag );
  Infer() : HIRPass( "infer" ) {}
};


} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_OPTIMIZATION_INFER_H_
