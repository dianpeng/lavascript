#ifndef CBASE_CONSTANT_FOLD_H_
#define CBASE_CONSTANT_FOLD_H_
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
// Helper to simplify the logic, it is used mainly after 1) type guard generated 2) inference succeeded
Expr* SimplifyLogic( Graph* , Expr* , Expr* , Binary::Operator , const IRInfoProvider& );
Expr* FoldIntrinsicCall( Graph* , ICall* );

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_CONSTANT_FOLD_H_
