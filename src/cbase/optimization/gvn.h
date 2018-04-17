#ifndef CBASE_OPTIMIZATION_GVN_H_
#define CBASE_OPTIMIZATION_GVN_H_
#include "src/cbase/hir-pass.h"

namespace lavascript {
namespace cbase {
namespace hir {

class Graph;

/**
 *  Global Value Numbering pass. It also contains some other simplification algorithm
 *  like value inference.
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
