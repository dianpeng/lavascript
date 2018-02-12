#include "gvn.h"

#include "src/cbase/hir-helper.h"
#include "src/cbase/hir-visitor.h"

#include <unordered_set>


namespace lavascript {
namespace cbase {
namespace hir {

namespace {

/**
 * GVN hash table.
 *
 * A gvn hash table is just a hash table wrapper around std::unoredered_map
 * plus specific function to determine whether we can add it into the hash
 * table and then do the correct job.
 *
 * A hir::Expr* can return 0 as its hash value which basically *disable* GVN
 * pass on this node.
 *
 * One thing to note , though GVN is disabled but reduction is still left as
 * it is. The GVN pass also perform expression level reduction.
 */

class GVNHashTable {
 public:
  // Find a expression from the GVNHashTable , if we cannot find it or the GVNHash
  // returns 0 , then we return NULL
  Expr* Find( Expr* ) const;

  // Try to insert this expression into GVNHashTable, if the target expression
  // doesn't support GVNHash by returning hash value to 0, then just return false,
  // otherwise return true.
  void Insert( Expr* );

 private:
  std::unordered_set<Expr*> table_;
};

Expr* GVNHashTable::Find( Expr* node ) const {
  auto itr = table_.find(node);
  return itr == table_.end() ? NULL : *itr;
}

void GVNHashTable::Insert( Expr* node ) {
  lava_verify(table_.insert(node).second);
}

} // namespace

bool GVN::Perform( Graph* graph , HIRPass::Flag flag ) {
  (void)flag;

  ControlFlowRPOIterator itr(*graph);
  GVNHashTable table;
  DynamicBitSet visited(graph->MaxID());

  for( ; itr.HasNext() ; itr.Move() ) {
    auto cf = itr.value();

    // all the operands node
    for( auto opr_itr( cf->operand_list()->GetForwardIterator() );
         opr_itr.HasNext(); opr_itr.Move() ) {
      auto expr = opr_itr.value();

      if(!visited[expr->id()]) {

        // number valuing
        for( ExprDFSIterator expr_itr(*graph,expr) ;
             expr_itr.HasNext(); expr_itr.Move() ) {

          auto subexpr = expr_itr.value();
          auto tar     = table.Find(subexpr);

          if(tar) {
            if(tar != subexpr) {
              subexpr->Replace(tar);          // okay, find a target, just replace the old one
              if(tar == expr) expr = subexpr; // it is replaced, so use the replaced value
            }
          } else {
            table.Insert(subexpr);
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
