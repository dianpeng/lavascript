#ifndef CBASE_FOLD_MEMORY_H_
#define CBASE_FOLD_MEMORY_H_
#include "src/cbase/hir.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// effect analyzing node
class Effect;

Expr* FoldIndexGet( Graph* , Expr* , Expr* , Effect* );
Expr* FoldPropGet ( Graph* , Expr* , Expr* , Effect* );

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_FOLD_MEMORY_H_
