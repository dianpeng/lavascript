#ifndef CBASE_OPTIMIZATION_GVN_H_
#define CBASE_OPTIMIZATION_GVN_H_
#include "src/cbase/hir-pass.h"

namespace lavascript {
namespace cbase {
namespace hir {

class Graph;


/**
 * This pass implements the basic Global Value Numbering algorithm described in
 * paper "Global Code Motion and Global Value Numbering" by Dr. Cliff Click
 *
 * Only global value numbering, global code motion is used during scheduling phase
 */
class GVN: public HIRPass {
 public:
  virtual bool Perform( Graph* , HIRPass::Flag );

  GVN() : HIRPass("global-value-numbering") {}
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_OPTIMIZATION_GVN_H_
