#ifndef CBASE_FOLD_ARITH_H_
#define CBASE_FOLD_ARITH_H_
#include "src/cbase/hir.h"

#include <functional>
#include <memory>

namespace lavascript {
namespace cbase {
namespace hir {

class Folder;

/**
 * Helper function to simplify some expression node. These functions
 * are used inside of GraphBuilder to do constant folding and this
 * could avoid checkpoint generation which will use lots of memory
 *
 * If function returns NULL , it means it cannot do any constant folding;
 * otherwise it will return the new node
 */

Expr* FoldUnary    ( Graph* , Unary::Operator , Expr* );
Expr* FoldBinary   ( Graph* , Binary::Operator, Expr* , Expr* );
Expr* FoldTernary  ( Graph* , Expr* , Expr* , Expr* );
Expr* SimplifyLogic( Graph* , Expr* , Expr* , Binary::Operator );

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_FOLD_ARITH_H_
