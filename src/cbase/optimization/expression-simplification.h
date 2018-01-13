#ifndef CBASE_OPTIMIZATION_EXPRESSION_SIMPLIFICATION_H_
#define CBASE_OPTIMIZATION_EXPRESSION_SIMPLIFICATION_H_

#include "src/cbase/hir.h"

#include <functional>

/**
 * expression simplification is a helper for GVN and it is called by GVN
 * directly. The normal way of performing it is as following :
 *
 * 1) GVN rewrite
 * 2) Expression Simplification
 * 3) If 2) succedeed , jump to 1) ; otherwise abort
 *
 */

namespace lavascript {
namespace cbase {
namespace hir {

class ExpressionSimplifier {
 public:
  enum Flag { NORMAL, DEBUG };

  bool Perform( Graph* , Expr* , Flag );
};


/**
 * Helper function to simplify some expression node. These functions
 * are used inside of GraphBuilder to do constant folding and this
 * could avoid checkpoint generation which will use lots of memory
 *
 * If function returns NULL , it means it cannot do any constant folding;
 * otherwise it will return the new node
 */
Expr* ExprSimplify( Graph* , Unary::Operator , Expr* , const std::function<IRInfo* ()>& );
Expr* ExprSimplify( Graph* , Binary::Operator, Expr* , Expr* , const std::function<IRInfo* ()>& );
Expr* ExprSimplify( Graph* , Expr* , Expr* , Expr* , const std::function<IRInfo* ()>& );

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_OPTIMIZATION_EXPRESSION_SIMPLIFICATION_H_
