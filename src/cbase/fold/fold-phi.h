#ifndef CBASE_FOLD_PHI_H_
#define CBASE_FOLD_PHI_H_
#include "src/cbase/hir.h"

#include <memory>

namespace lavascript {
namespace cbase      {
namespace hir        {

class Folder;

Expr* FoldPhi( Graph* , Expr* , Expr* , ControlFlow* );
Expr* FoldPhi( Graph* , Phi* );

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_FOLD_PHI_H_
