#include "fold-phi.h"
#include "fold-arith.h" // fold ternary

namespace lavascript {
namespace cbase      {
namespace hir        {

Expr* FoldPhi( Graph* graph , Expr* lhs , Expr* rhs , ControlFlow* region ) {
  // 1. if lhs and rhs are same, then just return lhs/rhs
  if(lhs->IsReplaceable(rhs)) {
    return lhs;
  }
  if(region->IsIf()) {
    auto inode = region->AsIf();
    // 2. try to fold it as a ternary if the cond is side effect free
    auto cond = inode->condition(); // get the condition
    auto    n = FoldTernary(graph,cond,lhs,rhs);
    if(n) return n;
  }
  return NULL;
}

Expr* FoldPhi( Phi* phi ) {
  if(phi->operand_list()->size() == 2) {
    auto lhs = phi->operand_list()->First();
    auto rhs = phi->operand_list()->Last();
    if(lhs->IsReplaceable(rhs)) return lhs;
  }
  return NULL;
}

} // namespace hir
} // namespace cbase
} // namespace lavascript