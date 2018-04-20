#ifndef CBASE_FOLD_MEMORY_H_
#define CBASE_FOLD_MEMORY_H_
#include "hir.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

Expr* FoldIndexGet( Graph* , IGet* );
Expr* FoldPropGet ( Graph* , PGet* );
bool  FoldIndexSet( Graph* , ISet* );
bool  FoldPropSet ( Graph* , PSet* );

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_FOLD_MEMORY_H_
