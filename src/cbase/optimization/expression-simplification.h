#ifndef CBASE_OPTIMIZATION_EXPRESSION_SIMPLIFICATION_H_
#define CBASE_OPTIMIZATION_EXPRESSION_SIMPLIFICATION_H_

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

class Graph;
class Expr;

class ExpressionSimplifier {
 public:
  enum Flag {
    NORMAL,
    DEBUG
  };

  bool Perform( Graph* , Expr* , Flag );
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_OPTIMIZATION_EXPRESSION_SIMPLIFICATION_H_
