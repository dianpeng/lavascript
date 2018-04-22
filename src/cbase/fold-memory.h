#ifndef CBASE_FOLD_MEMORY_H_
#define CBASE_FOLD_MEMORY_H_
#include "hir.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

Expr* FoldIndexGet( Graph* , Expr* , Expr* );
Expr* FoldPropGet ( Graph* , Expr* , Expr* );
Expr* FoldIndexSet( Graph* , Expr* , Expr* , Expr* );
Expr* FoldPropSet ( Graph* , Expr* , Expr* , Expr* );

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_FOLD_MEMORY_H_
