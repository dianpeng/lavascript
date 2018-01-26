#ifndef CBASE_CONSTANT_FOLD_H_
#define CBASE_CONSTANT_FOLD_H_
#include "src/cbase/hir.h"

#include <functional>

namespace lavascript {
namespace cbase {
namespace hir {

class StaticTypeInference;


/**
 * Helper function to simplify some expression node. These functions
 * are used inside of GraphBuilder to do constant folding and this
 * could avoid checkpoint generation which will use lots of memory
 *
 * If function returns NULL , it means it cannot do any constant folding;
 * otherwise it will return the new node
 */

Expr* ConstantFoldUnary  ( Graph* , Unary::Operator , Expr* ,
                                                      const StaticTypeInference& ,
                                                      const std::function<IRInfo* ()>& );

Expr* ConstantFoldBinary ( Graph* , Binary::Operator, Expr* , Expr* , const std::function<IRInfo* ()>& );

Expr* ConstantFoldTernary( Graph* , Expr* , Expr* , Expr* , const std::function<IRInfo* ()>& );

Expr* ConstantFoldIntrinsicCall( Graph* , ICall* );

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_CONSTANT_FOLD_H_
