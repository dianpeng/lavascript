#ifndef CBASE_FOLD_PHI_H_
#define CBASE_FOLD_PHI_H_
#include "hir.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

Expr* FoldPhi( Graph* , Expr* , Expr* , ControlFlow* );
Expr* FoldPhi( Phi* );

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_FOLD_PHI_H_
