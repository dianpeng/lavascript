#ifndef CBASE_FOLD_CAST_H_
#define CBASE_FOLD_CAST_H_

namespace lavascript {
namespace cbase      {
namespace hir        {

class Graph;
class Expr;

Expr* FoldCast( Graph* , Expr* );

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_FOLD_CAST_H_
