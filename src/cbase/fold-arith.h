#ifndef CBASE_FOLD_ARITH_H_
#define CBASE_FOLD_ARITH_H_
#include "src/cbase/hir.h"

#include <functional>

namespace lavascript {
namespace cbase {
namespace hir {

/**
 * Helper function to simplify some expression node. These functions
 * are used inside of GraphBuilder to do constant folding and this
 * could avoid checkpoint generation which will use lots of memory
 *
 * If function returns NULL , it means it cannot do any constant folding;
 * otherwise it will return the new node
 */

typedef std::function<IRInfo*()> IRInfoProvider;

Expr* FoldUnary  ( Graph* , Unary::Operator , Expr* , const IRInfoProvider& );
Expr* FoldBinary ( Graph* , Binary::Operator, Expr* , Expr* , const IRInfoProvider& );
Expr* FoldTernary( Graph* , Expr* , Expr* , Expr* , const IRInfoProvider& );
Expr* SimplifyLogic( Graph* , Expr* , Expr* , Binary::Operator , const IRInfoProvider& );

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_FOLD_ARITH_H_
