#ifndef CBASE_PASS_DCE_H_
#define CBASE_PASS_DCE_H_
#include "src/cbase/hir-pass.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

class Graph;

/**
 * Dead code elmination
 *
 * This DCE phase will help to remove unused/unneeded branch
 */
class DCE : public HIRPass {
 public:
  virtual bool Perform( Graph* , HIRPass::Flag );
  DCE() : HIRPass("dead-code-elimination") {}
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_PASS_DCE_H_
