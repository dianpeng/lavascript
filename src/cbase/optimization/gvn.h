#ifndef CBASE_OPTIMIZATION_GVN_H_
#define CBASE_OPTIMIZATION_GVN_H_
#include "src/cbase/pass.h"

/**
 * This pass implements the basic Global Value Numbering algorithm described in
 * paper "Global Code Motion and Global Value Numbering" by Dr. Cliff Click
 *
 * Apparently, with minor modification and adaption to our own framework
 */

namespace lavascript {
namespace cbase {
namespace hir {

class Graph;

class GVNPass : public HIRPass {
 public:
  virtual bool Perform( Graph* , HIRPass::Flag );
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_OPTIMIZATION_GVN_H_
