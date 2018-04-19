#include "gvn.h"
#include "src/cbase/hir-visitor.h"
#include "src/zone/table.h"

#include <unordered_set>

namespace lavascript {
namespace cbase {
namespace hir {

// This implements a simple *one pass* GVN instead of iterative version. Iterative GVN
// may capture more possible optimization at the cost of slow convergence time. It is
// relative simple to modify this algorithm to do interative version.
bool GVN::Perform( Graph* graph , HIRPass::Flag flag ) {
  static const std::size_t kStackSize = 1024;
  static const std::size_t kTableSize = 128 ; // make sure this number can utilize the stack size otherwise
                                              // it makes no sense to have stack zone
  (void)flag;

  DynamicBitSet                      visited(graph->MaxID());
  ::lavascript::zone::StackZone<kStackSize>             zone;
  ::lavascript::zone::Table<Expr*,Expr*,HIRExprHasher> table(&zone,kTableSize);

  lava_foreach( auto cf , ControlFlowRPOIterator(*graph) ) {
    lava_foreach( auto expr , cf->operand_list()->GetForwardIterator() ) {
    // all the operands node
      if(!visited[expr->id()]) {
        // number valuing
        lava_foreach( auto subexpr , ExprDFSIterator(*graph,expr) ) {
          auto itr = table.Find(subexpr);
          auto tar = itr.HasNext() ? itr.value() : NULL;
          if(tar) {
            if(tar != subexpr) {
              subexpr->Replace(tar);          // okay, find a target, just replace the old one
              if(tar == expr) expr = subexpr; // it is replaced, so use the replaced value
            }
          } else {
            lava_verify(table.Insert(&zone,subexpr,subexpr).second);
          }
        }
        // mark it to be visited
        visited[expr->id()] = true;
      }
    }
  }
  return true;
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
