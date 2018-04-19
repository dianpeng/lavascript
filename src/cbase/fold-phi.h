#ifndef CBASE_FOLD_PHI_H_
#define CBASE_FOLD_PHI_H_
#include "hir.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// Fold a phi node into another node. It doesn't only do fold but also do some other
// phi optimization and potentially could mark control flow region to be dead by set
// the condition node of control flow to be *false*
Expr* FoldPhi( Graph* , Expr* , Expr* , ControlFlow* , const IRInfoProvider& );

// Fold a phi that's already there. It just uses most conservative way to fold
Expr* FoldPhi( Phi* );

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_FOLD_PHI_H_
