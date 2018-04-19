#include "fold-phi.h"
#include "fold-arith.h" // fold ternary

namespace lavascript {
namespace cbase      {
namespace hir        {

Expr* FoldPhi( Graph* graph , Expr* lhs , Expr* rhs , ControlFlow* region , const IRInfoProvider& irinfo ) {
  // 1. if lhs and rhs are same, then just return lhs/rhs
  if(lhs->IsIdentical(rhs)) {
    return lhs;
  }
  if(region->IsIf()) {
    auto inode = region->AsIf();
    // 2. try to fold it as a ternary if the cond is side effect free
    auto cond = inode->condition(); // get the condition
    if(!cond->HasSideEffect()) {
      auto n = FoldTernary(graph,cond,lhs,rhs,irinfo);
      if(n) return n;
    }
  }
  return NULL;
}

Expr* FoldPhi( Phi* phi ) {
  if(!phi->HasSideEffect() && phi->operand_list()->size() == 2) {
    auto lhs = phi->operand_list()->First();
    auto rhs = phi->operand_list()->Last();
    if(lhs->IsIdentical(rhs)) return lhs;
  }
  return NULL;
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
